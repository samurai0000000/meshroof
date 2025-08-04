file(GLOB MAIN_SRCS "*.cxx")
idf_component_register(SRCS "${MAIN_SRCS}" REQUIRES pthread)
