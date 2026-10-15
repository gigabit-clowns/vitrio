# SPDX-License-Identifier: LGPL-2.1-or-later

find_package(Doxygen REQUIRED)

# The documentation is what the public headers say of themselves, with the
# README as its first page. Its name, version and brief are those of the
# project.
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
set(DOXYGEN_FILE_PATTERNS *.hpp *.md)
set(DOXYGEN_STRIP_FROM_PATH ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
set(DOXYGEN_STRIP_FROM_INC_PATH ${PROJECT_SOURCE_DIR}/include)

# A macro between a keyword and the name it declares hides the declaration
# from Doxygen, so those of the library are expanded to nothing.
set(DOXYGEN_MACRO_EXPANSION YES)
set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
set(
	DOXYGEN_PREDEFINED
	VITRIO_API=
	VITRIO_STD_BASE_INTERFACE=
	VITRIO_STD_MEMBER_INTERFACE=
)

# A comment that names a parameter that is not there, or refers to
# something that does not exist, fails the build of the documentation. What
# has no comment does not: the constructors and operators that only copy or
# compare are left without one, and so is what overrides an interface.
set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)

doxygen_add_docs(
	vitrio-docs
	${PROJECT_SOURCE_DIR}/README.md
	${PROJECT_SOURCE_DIR}/include
	COMMENT "Generating the documentation of the API"
)
