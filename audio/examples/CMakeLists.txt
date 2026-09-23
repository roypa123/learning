# One executable per example, all linking the library.
# Adding a new example is one line in this list.

set(EXAMPLES
    ch17_tour
    ch19_phasors
    ch20_lti
    ch21_convreverb
    ch22_fir
    ch23_biquad
    ch24_polezero
    ch25_fft
)

foreach(name ${EXAMPLES})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE audio)
endforeach()
