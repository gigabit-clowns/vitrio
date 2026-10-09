# SPDX-License-Identifier: LGPL-2.1-or-later

# Copies the dynamic libraries one directory holds into another.
#
#   cmake -DSOURCE_DIR=<from> -DDESTINATION_DIR=<to> -P <this file>
file(GLOB LIBRARIES "${SOURCE_DIR}/*.dll")
file(COPY ${LIBRARIES} DESTINATION "${DESTINATION_DIR}")
