if(USE_JAVA)
    set(common-java-files
        java_virtual_machine.cpp
    )
endif(USE_JAVA)

add_library(common SHARED
    aligning_writer_worker.cpp
    annotation_item.cpp
    annotation_item_manager.cpp
    annotator.cpp
    annotator_factory.cpp
    annotator_promise.cpp
    ant_like_path_glob.cpp
    bi_lang_specific_processor_file_fetcher.cpp
    bi_language_dependent_annotator_factory.cpp
    configurator.cpp
    cutter.cpp
    cutter_annotator_options.cpp
    encoding_converter.cpp
    escaping.cpp
    file_extractor.cpp
    file_fetcher.cpp
    file_recognizer.cpp
    file_storage.cpp
    guid_generator.cpp
    its_data.cpp
    language_dependent_annotator_factory.cpp
    lattice.cpp
    lattice_iter_writer.cpp
    lattice_worker.cpp
    lattice_writer_output_iterator.cpp
    layer_tag_collection.cpp
    layer_tag_manager.cpp
    layer_tag_mask.cpp
    lang_specific_processor_file_fetcher.cpp
    lemmatizer_output_iterator.cpp
    limit_checker.cpp
    logging.cpp
    md5.cpp
    morfologik_tags_parser.cpp
    non_annotator_promise.cpp
    object_cache.cpp
    processor.cpp
    processor_factory.cpp
    processor_file_fetcher.cpp
    processor_promise.cpp
    psi_exception.cpp
    psi_quoter.cpp
    put_zsyntree_into_lattice.cpp
    quoter.cpp
    regexp.cpp
    segment_glob.cpp
    stream_lattice_reader.cpp
    stream_helpers.cpp
    string_frag.cpp
    string_helpers.cpp
    string_inflection.cpp
    utt_like_quoter.cpp
    xml_property_tree.cpp
    zvalue.cpp
    ${common-java-files}
)

target_link_libraries(common pcre pcrecpp fex tiniconv uchardet)

include_directories("${psi-toolkit_SOURCE_DIR}/fex/fex")
link_directories("${psi-toolkit_BINARY_DIR}/fex/fex")

include_directories("${psi-toolkit_SOURCE_DIR}/tiniconv")
link_directories("${psi-toolkit_BINARY_DIR}/tiniconv")

include_directories("${psi-toolkit_SOURCE_DIR}/uchardet/src")
link_directories("${psi-toolkit_BINARY_DIR}/uchardet/src")

if(USE_JAVA)
    target_link_libraries(common ${JAVA_JVM_LIBRARY})
    add_subdirectory(j)
endif(USE_JAVA)

if(HAVE_LOG4CPP)
  target_link_libraries(common log4cpp)
endif(HAVE_LOG4CPP)

if(HAVE_LIBMAGIC)
    target_link_libraries(common ${LibMagic_LIBRARY})
endif(HAVE_LIBMAGIC)

add_subdirectory(t)

install(TARGETS common DESTINATION lib/)
