/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2022 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#ifndef BAREOS_STORED_APPEND_H_
#define BAREOS_STORED_APPEND_H_


#include "include/bareos.h"
#include "lib/bsock.h"
#include "stored/device_control_record.h"
#include "stored/record.h"

namespace storagedaemon {

class ProcessedFileData {
 public:
  explicit ProcessedFileData(DeviceRecord* record);
  DeviceRecord GetData();

 private:
  uint32_t volsessionid_{0};
  uint32_t volsessiontime_{0};
  int32_t fileindex_{0};
  int32_t stream_{0};
  uint32_t data_len_{0};
  std::string data_{};
};

class ProcessedFile {
 public:
  ProcessedFile() = default;
  explicit ProcessedFile(int32_t fileindex);

  void SendAttributesToDirector(JobControlRecord* jcr);
  void AddAttribute(DeviceRecord* record);
  int32_t GetFileIndex() { return fileindex_; }
  const std::vector<ProcessedFileData>& GetAttributes() const
  {
    return attributes_;
  }

 private:
  int32_t fileindex_{-1};
  std::vector<ProcessedFileData> attributes_;
};

bool DoAppendData(JobControlRecord* jcr, BareosSocket* bs, const char* what);
bool IsAttribute(DeviceRecord* record);
bool SendAttrsToDir(JobControlRecord* jcr, DeviceRecord* rec);

void DoBackupCheckpoint(JobControlRecord* jcr);
}  // namespace storagedaemon

#endif  // BAREOS_STORED_APPEND_H_
