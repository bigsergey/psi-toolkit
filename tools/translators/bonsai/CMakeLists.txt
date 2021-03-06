# Ignoring warnings from irstlm
add_definitions(-Wno-unused-but-set-variable -Wno-sign-compare -Wno-write-strings -Wno-unused-parameter -Wno-undef -Wno-parentheses)

include_directories("${psi-toolkit_SOURCE_DIR}/tools/parsers/puddle")
link_directories("${psi-toolkit_BINARY_DIR}/tools/parsers/puddle")

add_library(tools-translators-bonsai SHARED
    bonsai_runner.cpp
    bonsai.cpp
    translator/Translator.hpp
    irstlm/mfstream.cpp
    irstlm/lmmacro.cpp
    irstlm/htable.cpp
    irstlm/mempool.cpp
    irstlm/util.cpp
    irstlm/ngramtable.cpp
    irstlm/n_gram.cpp
    irstlm/lmtable.cpp
    irstlm/dictionary.cpp
    irstlm/ngramcache.cpp
    rules/lattice_wrapper.cpp
    rules/CompressedDAG.cpp
    rules/HuffedWords.cpp
    rules/RuleSet.cpp
    rules/RuleLoader.cpp
    rules/SimpleDAG.cpp
    translator/TransferTypes.cpp
    translator/Transformation.cpp
    translator/Translation.cpp
    translator/LmContainer.cpp
    translator/Translator.cpp
)

set(bonsai_SUBDIRS irstlm rules translator CACHE PATH "Bonsai subdirectories" FORCE)

foreach(subdir ${bonsai_SUBDIRS})
    include_directories("${psi-toolkit_SOURCE_DIR}/tools/translators/bonsai/${subdir}")
    link_directories("${psi-toolkit_BINARY_DIR}/tools/translators/bonsai/${subdir}")
    add_subdirectory(${subdir})
endforeach(subdir)

target_link_libraries(tools-translators-bonsai ${Boost_LIBRARIES} tools-parsers-puddle z common)

install(TARGETS tools-translators-bonsai DESTINATION lib/)
install(DIRECTORY data/ DESTINATION ${PSITOOLKIT_INSTALL_DATA_DIR}/bonsai-runner)

add_subdirectory(utils)
