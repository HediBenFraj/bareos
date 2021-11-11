#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2021 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

# find programs
find_program(AWK awk)
find_program(GAWK gawk)
if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set(AWK ${GAWK})
endif()

find_program(PIDOF pidof)
if(NOT PIDOF)
  set(PIDOF "")
endif()

find_program(PS ps)
if(PS)
  set(PSCMD ${PS})
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(PSCMD "${PS} -e")
  endif()
  if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    set(PSCMD "${PS} -e -o pid,comm")
  endif()
  if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(PSCMD "${PS} -ax -o pid,command")
  endif()
  if(${CMAKE_SYSTEM_NAME} MATCHES "HP-UX")
    set(PSCMD "UNIX95=1; ${PS} -e -o pid,comm")
  endif()

  set(PSCMD
      "${PSCMD}"
      PARENT_SCOPE
  )
endif()

find_program(PGREP pgrep)
find_program(RPCGEN rpcgen)
find_program(MTX mtx)
find_program(MT mt)

find_program(GCORE gcore)
find_program(GDB gdb)
find_program(DBX dbx)
find_program(MDB mdb)
find_program(S3CMD s3cmd)
find_program(MINIO minio)

if(POLICY CMP0109)
  cmake_policy(SET CMP0109 NEW)
  find_program(SUDO sudo)
else()
  set(SUDO
      "/usr/bin/sudo"
      PARENT_SCOPE
  )
endif(POLICY CMP0109)

# To test backups of mysql with percona xtrabackup, and mariadb with mariabackup,
# we need to find the specific binaries in order to run our test databases:

# For mysql:
# XTRABACKUP_BINARY
# MYSQL_DAEMON_BINARY
# MYSQL_CLIENT_BINARY

if(NOT DEFINED XTRABACKUP_BINARY)
endif()

if(NOT DEFINED MYSQL_DAEMON_BINARY)
endif()

if(NOT DEFINED MYSQL_CLIENT_BINARY)
endif()



# For mariadb:
# MARIABACKUP_BINARY
# MARIADB_DAEMON_BINARY
# MARIADB_CLIENT_BINARY

if(NOT DEFINED MARIABACKUP_BINARY)
endif()

if(NOT DEFINED MARIADB_DAEMON_BINARY)
endif()

if(NOT DEFINED MARIADB_CLIENT_BINARY)
endif()

if(NOT DEFINED MARIADB_MYSQL_INSTALL_DB_SCRIPT)
endif()
