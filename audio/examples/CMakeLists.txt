# One executable per example, all linking the library.
# Adding a new example is one line in this list.

set(EXAMPLES
    ch17_tour
)

foreach(name ${EXAMPLES})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE audio)
endforeach()
