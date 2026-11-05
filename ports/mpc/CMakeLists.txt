project(mpc VERSION 1.4.1)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})

set(MPC_HAVE_COMPLEX_H 1)
configure_file(repo/src/mpc.h.in ${GEN}/mpc.h @ONLY)
configure_file(repo/src/mpc-impl.h.in ${GEN}/mpc-impl.h @ONLY)

add_library(mpc SHARED
    repo/src/abs.c
    repo/src/acos.c
    repo/src/acosh.c
    repo/src/add.c
    repo/src/add_fr.c
    repo/src/add_si.c
    repo/src/add_ui.c
    repo/src/agm.c
    repo/src/arg.c
    repo/src/asin.c
    repo/src/asinh.c
    repo/src/atan.c
    repo/src/atanh.c
    repo/src/balls.c
    repo/src/clear.c
    repo/src/cmp_abs.c
    repo/src/cmp.c
    repo/src/cmp_si_si.c
    repo/src/conj.c
    repo/src/cos.c
    repo/src/cosh.c
    repo/src/div_2si.c
    repo/src/div_2ui.c
    repo/src/div.c
    repo/src/div_fr.c
    repo/src/div_ui.c
    repo/src/dot.c
    repo/src/eta.c
    repo/src/exp10.c
    repo/src/exp2.c
    repo/src/exp.c
    repo/src/fma.c
    repo/src/fr_div.c
    repo/src/fr_sub.c
    repo/src/get_prec2.c
    repo/src/get_prec.c
    repo/src/get_version.c
    repo/src/get_x.c
    repo/src/imag.c
    repo/src/init2.c
    repo/src/init3.c
    repo/src/inp_str.c
    repo/src/log10.c
    repo/src/log2.c
    repo/src/log.c
    repo/src/mem.c
    repo/src/mul_2si.c
    repo/src/mul_2ui.c
    repo/src/mul.c
    repo/src/mul_fr.c
    repo/src/mul_i.c
    repo/src/mul_si.c
    repo/src/mul_ui.c
    repo/src/neg.c
    repo/src/norm.c
    repo/src/out_str.c
    repo/src/pow.c
    repo/src/pow_d.c
    repo/src/pow_fr.c
    repo/src/pow_ld.c
    repo/src/pow_si.c
    repo/src/pow_ui.c
    repo/src/pow_z.c
    repo/src/proj.c
    repo/src/radius.c
    repo/src/real.c
    repo/src/rootofunity.c
    repo/src/set.c
    repo/src/set_prec.c
    repo/src/set_str.c
    repo/src/set_x.c
    repo/src/set_x_x.c
    repo/src/sin.c
    repo/src/sin_cos.c
    repo/src/sinh.c
    repo/src/sqr.c
    repo/src/sqrt.c
    repo/src/strtoc.c
    repo/src/sub.c
    repo/src/sub_fr.c
    repo/src/sub_ui.c
    repo/src/sum.c
    repo/src/swap.c
    repo/src/tan.c
    repo/src/tanh.c
    repo/src/uceil_log2.c
    repo/src/ui_div.c
    repo/src/ui_ui_sub.c
    repo/src/urandom.c
)

set_target_properties(mpc PROPERTIES OUTPUT_NAME "mpc-3" ARCHIVE_OUTPUT_NAME "mpc" PREFIX "lib")

target_include_directories(mpc PRIVATE
    ${GEN}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
)

target_include_directories(mpc PUBLIC ${GEN})

target_compile_options(mpc PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-ignored-attributes
)

target_link_libraries(mpc PUBLIC mpfr)
target_link_libraries(mpc PRIVATE moxnox)

install(TARGETS mpc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES ${GEN}/mpc.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

set(prefix "")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(VERSION ${PROJECT_VERSION})
set(REQUIRES_MPFR_VERSION 4.1.0)
set(REQUIRES_GMP_VERSION 5.0.0)
configure_file(repo/mpc.pc.in ${GEN}/mpc.pc @ONLY)
install(FILES ${GEN}/mpc.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
