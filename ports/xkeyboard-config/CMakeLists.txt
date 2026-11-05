project(xkeyboard-config VERSION 2.48)

find_program(HOST_PYTHON3 python3 REQUIRED)
find_program(HOST_PERL perl REQUIRED)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/rules)
file(MAKE_DIRECTORY ${GEN}/symbols)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
set(XKB_BASE ${CMAKE_INSTALL_PREFIX}/share/xkeyboard-config-2)

set(RULES_GENERATOR
    repo/rules/generator/__init__.py
    repo/rules/generator/__main__.py
    repo/rules/generator/compatibility.py
    repo/rules/generator/options.py
    repo/rules/generator/parser.py
    repo/rules/generator/registry.py
    repo/rules/generator/template.py
)

set(COMPAT_MAPPINGS
    repo/rules/compat/layoutsMapping.lst
    repo/rules/compat/variantsMapping.lst
    repo/rules/compat/variantsMapping-vendors.lst
)

foreach(ruleset base evdev)
    add_custom_command(
        OUTPUT ${GEN}/rules/${ruleset}
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${REPO}
                ${HOST_PYTHON3} -B -m rules.generator rules --ruleset ${ruleset}
                --version v2 --output ${GEN}/rules/${ruleset} --compat
                ${REPO}/rules/rules.in
        DEPENDS repo/rules/rules.in ${RULES_GENERATOR} ${COMPAT_MAPPINGS}
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${GEN}/rules/${ruleset}.lst
        COMMAND sh -c "${HOST_PERL} '${REPO}/rules/xml2lst.pl' '${REPO}/rules/base.xml' > '${GEN}/rules/${ruleset}.lst'"
        DEPENDS repo/rules/xml2lst.pl repo/rules/base.xml
        VERBATIM
    )
endforeach()

set(COMPAT_SYMBOLS ara de fi gr in ir pk ro ru)

set(COMPAT_SYMBOLS_OUTPUTS)
set(COMPAT_SYMBOLS_INPUTS)
foreach(sym ${COMPAT_SYMBOLS})
    list(APPEND COMPAT_SYMBOLS_OUTPUTS ${GEN}/symbols/${sym})
    list(APPEND COMPAT_SYMBOLS_INPUTS repo/symbols/${sym})
endforeach()

add_custom_command(
    OUTPUT ${COMPAT_SYMBOLS_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${REPO}
            ${HOST_PYTHON3} -B -m rules.generator symbols ${GEN}/symbols
    DEPENDS ${COMPAT_SYMBOLS_INPUTS} ${RULES_GENERATOR} ${COMPAT_MAPPINGS}
    VERBATIM
)

add_custom_target(xkeyboard-config ALL DEPENDS
    ${GEN}/rules/base
    ${GEN}/rules/evdev
    ${GEN}/rules/base.lst
    ${GEN}/rules/evdev.lst
    ${COMPAT_SYMBOLS_OUTPUTS}
)

install(DIRECTORY repo/compat/ DESTINATION ${XKB_BASE}/compat)
install(DIRECTORY repo/geometry/ DESTINATION ${XKB_BASE}/geometry)
install(DIRECTORY repo/keycodes/ DESTINATION ${XKB_BASE}/keycodes)
install(DIRECTORY repo/types/ DESTINATION ${XKB_BASE}/types
    PATTERN "custom" EXCLUDE
)

list(JOIN COMPAT_SYMBOLS "|" COMPAT_SYMBOLS_REGEX)
install(DIRECTORY repo/symbols/ DESTINATION ${XKB_BASE}/symbols
    REGEX "/symbols/(caps|esperanto|grp|japan|korean|lv2|lv3|lv5|meson\\.build)$" EXCLUDE
    REGEX "/symbols/(${COMPAT_SYMBOLS_REGEX})$" EXCLUDE
)
install(FILES ${COMPAT_SYMBOLS_OUTPUTS} DESTINATION ${XKB_BASE}/symbols)

install(FILES
    repo/rules/README
    repo/rules/xkb.dtd
    repo/rules/xfree98
    repo/rules/base.xml
    repo/rules/base.extras.xml
    ${GEN}/rules/base
    ${GEN}/rules/evdev
    ${GEN}/rules/base.lst
    ${GEN}/rules/evdev.lst
    DESTINATION ${XKB_BASE}/rules
)
install(FILES repo/rules/base.xml DESTINATION ${XKB_BASE}/rules RENAME evdev.xml)
install(FILES repo/rules/base.extras.xml DESTINATION ${XKB_BASE}/rules RENAME evdev.extras.xml)

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_PREFIX}/share/X11)
install(CODE "file(CREATE_LINK /share/xkeyboard-config-2 \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/X11/xkb\" SYMBOLIC)")

configure_file(xkeyboard-config.pc.in ${GEN}/xkeyboard-config-${PROJECT_VERSION_MAJOR}.pc @ONLY)
install(FILES ${GEN}/xkeyboard-config-${PROJECT_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pkgconfig)
install(CODE "file(CREATE_LINK xkeyboard-config-${PROJECT_VERSION_MAJOR}.pc \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/pkgconfig/xkeyboard-config.pc\" SYMBOLIC)")
