# Chapter 64 — Graph-Based Audio Engines

> Chapter 17's `Chain` runs processors in sequence. Real systems need branching, merging,
> feedback, sidechains and buses — a **graph**. This chapter builds one, and deals with the two
> genuinely hard problems: ordering and latency.

---

## 64.1 Why a graph

A mixing console, a modular synth, a game audio engine and a DAW are all the same structure:
**nodes that produce or process audio, connected by edges that carry it.**

```
   [synth] ──┬──► [EQ] ──► [comp] ──┬──► [master] ──► [out]
             │                      │
             └──► [reverb send] ────┘
                       ▲
   [drums] ──► [EQ] ───┴──────────────► [master]
```

What a graph gives you that a chain does not:

- **Branching** — one source to several destinations
- **Merging** — several sources into one
- **Sends and returns** — a parallel path that rejoins
- **Sidechains** — an edge that carries control rather than audio
- **Dynamic structure** — the user rewires it while it runs

---

## 64.2 The node

```cpp
class Node
{
public:
    virtual ~Node() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void process(const std::vector<const AudioBuffer*>& inputs,
                         AudioBuffer& output,
                         int numFrames) = 0;
    virtual void reset() = 0;

    virtual int numInputs()  const { return 1; }
    virtual int numOutputs() const { return 1; }

    // Latency this node introduces, in samples. Critical -- see s64.5.
    virtual int latencySamples() const { return 0; }

    virtual const char* name() const = 0;

    int id = -1;
};
```

**`inputs` is a vector of pointers** because a node may have several, and some may be
disconnected (`nullptr`). A mixer node sums whatever is present; a sidechain compressor uses
input 0 as audio and input 1 as the detector.

---

## 64.3 Topological sort

**The central problem: what order do you process the nodes in?**

A node cannot run until every node feeding it has run. That is a **topological sort** of the
graph, and it must be computed whenever the connections change — on the main thread, never in
the callback.

```cpp
std::vector<int> topologicalSort(const Graph& graph, bool& hasCycle)
{
    std::vector<int> order;
    std::unordered_map<int, int> inDegree;

    // Count incoming edges for each node.
    for (const auto& [id, node] : graph.nodes)
        inDegree[id] = 0;
    for (const auto& edge : graph.edges)
        ++inDegree[edge.destNode];

    // Start with every node that has no inputs.
    std::queue<int> ready;
    for (const auto& [id, degree] : inDegree)
        if (degree == 0) ready.push(id);

    while (!ready.empty())
    {
        const int id = ready.front();
        ready.pop();
        order.push_back(id);

        // Remove this node's outgoing edges.
        for (const auto& edge : graph.edges)
            if (edge.sourceNode == id)
                if (--inDegree[edge.destNode] == 0)
                    ready.push(edge.destNode);
    }

    // If we did not visit every node, there is a cycle.
    hasCycle = (order.size() != graph.nodes.size());
    return order;
}
```

**This is Kahn's algorithm**, and it detects cycles as a side effect: if nodes remain with
non-zero in-degree, they are part of a loop.

**Compute the order on the main thread and publish it atomically** (Chapter 60's pointer swap).
The audio thread reads a pre-computed list and walks it.

---

## 64.4 Feedback

A cycle is not necessarily an error — feedback is musically essential. But a graph with a cycle
cannot be topologically sorted, because no node in the cycle can go first.

**The solution: break the cycle with a one-block delay.**

```cpp
class FeedbackNode : public Node
{
public:
    void prepare(double, int maxBlockSize) override
    {
        previousBlock_.resize(2, maxBlockSize);
        previousBlock_.clear();
    }

    void process(const std::vector<const AudioBuffer*>& inputs,
                 AudioBuffer& output, int numFrames) override
    {
        // Output the PREVIOUS block -- this breaks the dependency.
        for (int c = 0; c < output.numChannels(); ++c)
            std::copy_n(previousBlock_.channel(c).begin(), numFrames,
                        output.channel(c).begin());

        // Store this block for next time.
        if (inputs[0])
            for (int c = 0; c < previousBlock_.numChannels(); ++c)
                std::copy_n(inputs[0]->channel(c).begin(), numFrames,
                            previousBlock_.channel(c).begin());
    }

    int latencySamples() const override { return blockSize_; }

private:
    AudioBuffer previousBlock_;
    int blockSize_ = 512;
};
```

**The cost is one block of latency in the loop** — 11.6 ms at 512 frames. For a reverb send that
is irrelevant; for a tight feedback effect it is too much.

**The alternative for tight feedback:** keep the loop *inside* a single node, where you control
the sample-by-sample ordering. Chapter 47's FDN does exactly this — sixteen delay lines feeding
each other within one node, at single-sample granularity.

> **The general principle:** feedback at the graph level costs a block; feedback inside a node
> costs a sample. Put anything that needs tight feedback inside one node.

---

## 64.5 Latency compensation

**The hardest problem in graph audio**, and the one most likely to be wrong.

If two parallel paths have different latencies, they arrive misaligned. Chapter 2 tells you what
happens when you sum a signal with a delayed copy of itself: **comb filtering**.

```
   [source] ──┬──► [EQ, 0 samples] ─────────────► [mix] ──► out
              │
              └──► [linear-phase EQ, 1024] ─────► [mix]

   The two paths are 1024 samples apart. Summed, they comb-filter with
   notches every 43 Hz. This is audible and it sounds like a phaser
   that nobody asked for.
```

**The fix: compute each node's cumulative latency and delay the faster paths to match.**

```cpp
void computeLatencyCompensation(Graph& graph, const std::vector<int>& order)
{
    std::unordered_map<int, int> cumulative;

    // Walk in topological order: every input is already computed.
    for (int id : order)
    {
        Node& node = *graph.nodes[id];

        // This node cannot produce output until its SLOWEST input has arrived.
        int maxInput = 0;
        for (const auto& edge : graph.edges)
            if (edge.destNode == id)
                maxInput = std::max(maxInput, cumulative[edge.sourceNode]);

        cumulative[id] = maxInput + node.latencySamples();

        // Delay every input that is EARLIER than the slowest.
        for (auto& edge : graph.edges)
            if (edge.destNode == id)
                edge.compensationDelay = maxInput - cumulative[edge.sourceNode];
    }
}
```

**Each edge gets a delay line** sized to its compensation amount. Edges from the slowest path get
zero; faster paths get delayed to match.

**Three things that make this go wrong in practice:**

**1. Nodes that report the wrong latency.** A plugin with lookahead that reports 0 will comb
against everything. Chapter 51's limiter has `latencySamples()` for exactly this reason.

**2. Latency that changes.** A plugin switching between linear-phase and minimum-phase modes
changes its latency mid-session. The graph must recompute — and doing so mid-stream causes a
glitch, which is why hosts usually require a brief stop.

**3. Feedback loops.** A cycle has no well-defined cumulative latency. The `FeedbackNode`'s
block delay must be accounted for explicitly.

**The total latency of the graph** is the maximum cumulative latency at any output node, and it
is what the host reports to the user.

---

## 64.6 Buffer management

Every node needs an output buffer. Allocating one per node wastes memory and cache; allocating
per block violates Chapter 59.

**The solution: a pool of buffers, assigned by the scheduler.**

```cpp
class BufferPool
{
public:
    void prepare(int numBuffers, int numChannels, int maxBlockSize)
    {
        buffers_.clear();
        for (int i = 0; i < numBuffers; ++i)
            buffers_.emplace_back(numChannels, maxBlockSize, sampleRate_);
        inUse_.assign(static_cast<size_t>(numBuffers), false);
    }

    AudioBuffer* acquire()
    {
        for (size_t i = 0; i < inUse_.size(); ++i)
            if (!inUse_[i]) { inUse_[i] = true; return &buffers_[i]; }
        return nullptr;
    }

    void release(AudioBuffer* b) { /* mark free */ }

private:
    std::vector<AudioBuffer> buffers_;
    std::vector<bool> inUse_;
    double sampleRate_ = kDefaultRate;
};
```

**A buffer can be reused as soon as every node that reads it has run.** Computing that — a
liveness analysis over the topological order — lets a large graph run with surprisingly few
buffers. A 50-node graph typically needs 5–8.

**In-place processing** where possible: a node with one input and one output that does not need
the original can write over its input buffer, avoiding a copy entirely. Most filters and gains
can do this; anything with a dry/wet mix cannot.

---

## 64.7 Multi-threading the graph

Independent branches can run on separate cores.

```
   [synth A] ──► [fx A] ──┐
                          ├──► [master]
   [synth B] ──► [fx B] ──┘

   A and B are independent: two threads can process them concurrently.
```

**The approach:**

1. Group nodes into **levels** — nodes at the same topological depth have no dependencies on each
   other.
2. Process each level in parallel across a thread pool.
3. Synchronise between levels.

**Three cautions, and they are significant:**

**Synchronisation costs.** A barrier between levels costs microseconds. With a 5.8 ms budget and
ten levels, that is manageable; with a hundred levels it is not. Coarse parallelism (whole
branches) beats fine parallelism (individual nodes).

**Worker threads must also be real-time priority**, and must never allocate or block —
Chapter 59's rules apply to all of them, not just the callback thread.

**The worst case can get worse.** If one branch is much more expensive than the others, the
threads wait for it and you gain nothing while paying the synchronisation cost. Load balancing
across branches matters.

**The realistic assessment:** multi-threading a graph gives a genuine 2–3× on a 4-core machine
*for large graphs*. For a small graph the overhead dominates. Most DAWs parallelise per-track,
which is naturally coarse-grained, rather than per-node.

---

## 64.8 Modifying the graph safely

The user adds a plugin while audio is running. This must not allocate on the audio thread, must
not tear the graph mid-block, and must not click.

**The standard approach — build a new graph and swap it atomically:**

```cpp
void addNode(std::unique_ptr<Node> node)
{
    // 1. Copy the current graph description (MAIN THREAD).
    auto newGraph = std::make_unique<CompiledGraph>(*currentGraph_);

    // 2. Modify it.
    newGraph->addNode(std::move(node));

    // 3. Recompute order, latency, buffer assignment -- all expensive,
    //    all on the main thread.
    newGraph->compile();

    // 4. Prepare every node (allocates).
    newGraph->prepare(sampleRate_, maxBlockSize_);

    // 5. Publish atomically (Chapter 60).
    publisher_.publish(std::move(newGraph));

    // 6. The old graph is retired and freed later, once the audio thread
    //    can no longer be using it.
}
```

**The audio thread reads the current pointer once per block** and uses that graph for the whole
block. It never sees a partially-modified structure.

**State transfer is the remaining problem.** The new graph's nodes are fresh, with zeroed filter
state and empty delay lines. Switching mid-audio means every reverb tail stops and every filter
restarts — an audible discontinuity.

**Two mitigations:** reuse the existing node objects where they are unchanged (only new and
removed nodes are fresh), or crossfade between the old and new graphs over ~20 ms, which requires
running both briefly.

---

## 64.9 Exercises

**64.1** Implement the node interface and a few nodes: oscillator, gain, filter, mixer.

**64.2** Implement Kahn's topological sort. Test it on a graph with a diamond shape
(A→B, A→C, B→D, C→D).

**64.3** Add cycle detection. Create a cycle and verify it is reported.

**64.4** Implement the feedback node. Build a delay with feedback through the graph and measure
the actual delay time — does it include the block?

**64.5** *Deliberate breakage.* Build two parallel paths, one with a 1024-sample latency node,
and sum them. Measure the frequency response and find the comb notches.

**64.6** Implement latency compensation. Verify the same graph now sums flat.

**64.7** Build a graph with a node that lies about its latency. How does it manifest?

**64.8** Implement the buffer pool with liveness analysis. How many buffers does a 20-node graph
need?

**64.9** Implement in-place processing where possible. Measure the reduction in buffer count and
in memory traffic.

**64.10** Implement atomic graph swapping. Add and remove nodes while audio is playing and verify
there are no glitches or races (thread sanitizer).

---

### Chapter summary

- A graph is nodes connected by edges, giving branching, merging, sends, sidechains and dynamic
  structure that a chain cannot.
- **Topological sort** (Kahn's algorithm) determines the processing order and **detects cycles as
  a side effect**. Compute it on the main thread; publish it atomically.
- **Feedback at the graph level costs one block** of latency via a `FeedbackNode`. **Feedback
  inside a node costs one sample.** Put anything needing tight feedback inside a single node —
  which is what Chapter 47's FDN does.
- **Latency compensation is the hardest part.** Parallel paths with different latencies
  **comb-filter** when summed. Compute cumulative latency in topological order and delay the
  faster paths. Nodes that misreport their latency break this silently.
- Use a **buffer pool** with liveness analysis — a 50-node graph typically needs 5–8 buffers —
  and process **in place** where a node does not need its input preserved.
- **Multi-threading** works on independent branches, grouped into levels. Coarse parallelism
  (branches or tracks) beats fine (nodes), because synchronisation costs microseconds per level.
  Worker threads need the same real-time discipline.
- **Modify the graph by building a new one and swapping atomically.** The remaining problem is
  **state transfer** — fresh nodes have empty filters and delay lines — mitigated by reusing
  unchanged nodes or crossfading between graphs.

**Next:** [Chapter 65 — Audio Plugins: VST3, AU, and CLAP](65-audio-plugins.md)
