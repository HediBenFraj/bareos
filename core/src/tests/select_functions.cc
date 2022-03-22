/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "dird/ua_select.h"
#include "include/jcr.h"

using namespace directordaemon;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon


TEST(getJobStatus, returnsEmptyVectorWhenNoArgumentsAreGiven)
{
  UaContext* ua = nullptr;
  JobControlRecord jcr;
  ua = new_ua_context(&jcr);
  std::vector<std::string> jobstatus_list = GetUserJobStatusSelection(ua);

  EXPECT_EQ(jobstatus_list.size(), 0);
}


TEST(getJobStatus, returnNothingIfJobStatusArgumentIsNull)
{
  UaContext* ua = nullptr;
  JobControlRecord jcr;
  ua = new_ua_context(&jcr);
  std::string command = "list jobs jobstatus=T";

  PmStrcpy(ua->cmd, command.c_str());

  ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);

  std::vector<std::string> jobstatus_list = GetUserJobStatusSelection(ua);
  EXPECT_EQ(jobstatus_list.size(), 1);
}

// TEST(getJobStatus, returnSameNumberOfElementsInParams)
//{
//  UaContext* ua = nullptr;
//  JobControlRecord jcr;
//  ua = new_ua_context(&jcr);
//  std::vector<std::string> jobstatus_list = GetUserJobStatusSelection(ua);

//  EXPECT_EQ(jobstatus_list.size(), ua.argc);
//}
