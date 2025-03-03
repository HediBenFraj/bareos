/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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
// Kern Sibbald, MM
/**
 * @file
 * Test program for find files
 */

#include "include/bareos.h"
#include "dird/dird_conf.h"
#include "findlib/find.h"
#include "lib/mntent_cache.h"
#include "include/ch.h"
#include "filed/fd_plugins.h"
#include "lib/parse_conf.h"
#include "dird/jcr_util.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/jcr_private.h"
#include "lib/recent_job_results_list.h"
#include "findlib/attribs.h"
#include "filed/fileset.h"
#include "lib/util.h"
#include "lib/tree.h"

#if defined(HAVE_WIN32)
#  define isatty(fd) (fd == 0)
#endif


using namespace directordaemon;

void TestfindFreeJcr(JobControlRecord* jcr)
{
  Dmsg0(200, "Start testfind FreeJcr\n");

  if (jcr->impl) {
    delete jcr->impl;
    jcr->impl = nullptr;
  }

  Dmsg0(200, "End testfind FreeJcr\n");
}


/* Dummy functions */
void GeneratePluginEvent(JobControlRecord*, filedaemon::bEventType, void*) {}
extern bool ParseDirConfig(const char* configfile, int exit_code);

/* Global variables */
static int num_files = 0;
static int max_file_len = 0;
static int max_path_len = 0;
static int trunc_fname = 0;
static int trunc_path = 0;
static int attrs = 0;

static JobControlRecord* jcr;

static int PrintFile(JobControlRecord* jcr, FindFilesPacket* ff, bool);
static void CountFiles(FindFilesPacket* ff);
static bool CopyFileset(FindFilesPacket* ff, JobControlRecord* jcr);
static void SetOptions(findFOPTS* fo, const char* opts);

static void usage()
{
  fprintf(
      stderr,
      _("\n"
        "Usage: testfind [-d debug_level] [-] [pattern1 ...]\n"
        "       -a          print extended attributes (Win32 debug)\n"
        "       -d <nn>     set debug level to <nn>\n"
        "       -dt         print timestamp in debug output\n"
        "       -c          specify config file containing FileSet resources\n"
        "       -f          specify which FileSet to use\n"
        "       -?          print this message.\n"
        "\n"
        "Patterns are used for file inclusion -- normally directories.\n"
        "Debug level >= 1 prints each file found.\n"
        "Debug level >= 10 prints path/file for catalog.\n"
        "Errors are always printed.\n"
        "Files/paths truncated is the number of files/paths with len > 255.\n"
        "Truncation is only in the catalog.\n"
        "\n"));

  exit(1);
}


int main(int argc, char* const* argv)
{
  FindFilesPacket* ff;
  const char* configfile = ConfigurationParser::GetDefaultConfigDir();
  const char* fileset_name = "SelfTest";
  int ch, hard_links;

  OSDependentInit();

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  while ((ch = getopt(argc, argv, "ac:d:f:?")) != -1) {
    switch (ch) {
      case 'a': /* print extended attributes *debug* */
        attrs = 1;
        break;

      case 'c': /* set debug level */
        configfile = optarg;
        break;

      case 'd': /* set debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 'f': /* exclude patterns */
        fileset_name = optarg;
        break;

      case '?':
      default:
        usage();
    }
  }

  argc -= optind;
  argv += optind;

  my_config = InitDirConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  MessagesResource* msg;

  foreach_res (msg, R_MSGS) {
    InitMsg(NULL, msg);
  }

  jcr = NewDirectorJcr(TestfindFreeJcr);
  jcr->impl->res.fileset
      = (FilesetResource*)my_config->GetResWithName(R_FILESET, fileset_name);

  if (jcr->impl->res.fileset == NULL) {
    fprintf(stderr, "%s: Fileset not found\n", fileset_name);

    FilesetResource* var;

    fprintf(stderr, "Valid FileSets:\n");

    foreach_res (var, R_FILESET) {
      fprintf(stderr, "    %s\n", var->resource_name_);
    }

    exit(1);
  }

  ff = init_find_files();

  CopyFileset(ff, jcr);

  FindFiles(jcr, ff, PrintFile, NULL);

  FreeJcr(jcr);
  if (my_config) {
    delete my_config;
    my_config = NULL;
  }

  RecentJobResultsList::Cleanup();
  CleanupJcrChain();

  /* Clean up fileset */
  findFILESET* fileset = ff->fileset;

  if (fileset) {
    int i, j, k;
    /* Delete FileSet Include lists */
    for (i = 0; i < fileset->include_list.size(); i++) {
      findIncludeExcludeItem* incexe
          = (findIncludeExcludeItem*)fileset->include_list.get(i);
      for (j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
        for (k = 0; k < fo->regex.size(); k++) {
          regfree((regex_t*)fo->regex.get(k));
        }
        fo->regex.destroy();
        fo->regexdir.destroy();
        fo->regexfile.destroy();
        fo->wild.destroy();
        fo->wilddir.destroy();
        fo->wildfile.destroy();
        fo->wildbase.destroy();
        fo->fstype.destroy();
        fo->Drivetype.destroy();
      }
      incexe->opts_list.destroy();
      incexe->name_list.destroy();
    }
    fileset->include_list.destroy();

    /* Delete FileSet Exclude lists */
    for (i = 0; i < fileset->exclude_list.size(); i++) {
      findIncludeExcludeItem* incexe
          = (findIncludeExcludeItem*)fileset->exclude_list.get(i);
      for (j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
        fo->regex.destroy();
        fo->regexdir.destroy();
        fo->regexfile.destroy();
        fo->wild.destroy();
        fo->wilddir.destroy();
        fo->wildfile.destroy();
        fo->wildbase.destroy();
        fo->fstype.destroy();
        fo->Drivetype.destroy();
      }
      incexe->opts_list.destroy();
      incexe->name_list.destroy();
    }
    fileset->exclude_list.destroy();
    free(fileset);
  }
  ff->fileset = NULL;
  hard_links = TermFindFiles(ff);

  printf(_("\n"
           "Total files    : %d\n"
           "Max file length: %d\n"
           "Max path length: %d\n"
           "Files truncated: %d\n"
           "Paths truncated: %d\n"
           "Hard links     : %d\n"),
         num_files, max_file_len, max_path_len, trunc_fname, trunc_path,
         hard_links);

  FlushMntentCache();

  TermMsg();

  exit(0);
}


static int PrintFile(JobControlRecord*, FindFilesPacket* ff, bool)
{
  switch (ff->type) {
    case FT_LNKSAVED:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Lnka: %s -> %s\n", ff->fname, ff->link);
      }
      break;
    case FT_REGE:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Empty: %s\n", ff->fname);
      }
      CountFiles(ff);
      break;
    case FT_REG:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf(_("Reg: %s\n"), ff->fname);
      }
      CountFiles(ff);
      break;
    case FT_LNK:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Lnk: %s -> %s\n", ff->fname, ff->link);
      }
      CountFiles(ff);
      break;
    case FT_DIRBEGIN:
      return 1;
    case FT_NORECURSE:
    case FT_NOFSCHG:
    case FT_INVALIDFS:
    case FT_INVALIDDT:
    case FT_DIREND:
      if (debug_level) {
        char errmsg[100] = "";
        if (ff->type == FT_NORECURSE) {
          bstrncpy(errmsg, _("\t[will not descend: recursion turned off]"),
                   sizeof(errmsg));
        } else if (ff->type == FT_NOFSCHG) {
          bstrncpy(errmsg,
                   _("\t[will not descend: file system change not allowed]"),
                   sizeof(errmsg));
        } else if (ff->type == FT_INVALIDFS) {
          bstrncpy(errmsg, _("\t[will not descend: disallowed file system]"),
                   sizeof(errmsg));
        } else if (ff->type == FT_INVALIDDT) {
          bstrncpy(errmsg, _("\t[will not descend: disallowed drive type]"),
                   sizeof(errmsg));
        }
        printf("%s%s%s\n", (debug_level > 1 ? "Dir: " : ""), ff->fname, errmsg);
      }
      ff->type = FT_DIREND;
      CountFiles(ff);
      break;
    case FT_SPEC:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Spec: %s\n", ff->fname);
      }
      CountFiles(ff);
      break;
    case FT_NOACCESS:
      printf(_("Err: Could not access %s: %s\n"), ff->fname, strerror(errno));
      break;
    case FT_NOFOLLOW:
      printf(_("Err: Could not follow ff->link %s: %s\n"), ff->fname,
             strerror(errno));
      break;
    case FT_NOSTAT:
      printf(_("Err: Could not stat %s: %s\n"), ff->fname, strerror(errno));
      break;
    case FT_NOCHG:
      printf(_("Skip: File not saved. No change. %s\n"), ff->fname);
      break;
    case FT_ISARCH:
      printf(_("Err: Attempt to backup archive. Not saved. %s\n"), ff->fname);
      break;
    case FT_NOOPEN:
      printf(_("Err: Could not open directory %s: %s\n"), ff->fname,
             strerror(errno));
      break;
    default:
      printf(_("Err: Unknown file ff->type %d: %s\n"), ff->type, ff->fname);
      break;
  }
  if (attrs) {
    char attr[200];
    encode_attribsEx(NULL, attr, ff);
    if (*attr != 0) { printf("AttrEx=%s\n", attr); }
    //    set_attribsEx(NULL, ff->fname, NULL, NULL, ff->type, attr);
  }
  return 1;
}

static void CountFiles(FindFilesPacket* ar)
{
  int fnl, pnl;
  char *l, *p;
  PoolMem file(PM_FNAME);
  PoolMem spath(PM_FNAME);

  num_files++;

  /* Find path without the filename.
   * I.e. everything after the last / is a "filename".
   * OK, maybe it is a directory name, but we treat it like
   * a filename. If we don't find a / then the whole name
   * must be a path name (e.g. c:).
   */
  for (p = l = ar->fname; *p; p++) {
    if (IsPathSeparator(*p)) { l = p; /* set pos of last slash */ }
  }
  if (IsPathSeparator(*l)) { /* did we find a slash? */
    l++;                     /* yes, point to filename */
  } else {                   /* no, whole thing must be path name */
    l = p;
  }

  /* If filename doesn't exist (i.e. root directory), we
   * simply create a blank name consisting of a single
   * space. This makes handling zero length filenames
   * easier.
   */
  fnl = p - l;
  if (fnl > max_file_len) { max_file_len = fnl; }
  if (fnl > 255) {
    printf(_("===== Filename truncated to 255 chars: %s\n"), l);
    fnl = 255;
    trunc_fname++;
  }

  if (fnl > 0) {
    PmStrcpy(file, l); /* copy filename */
  } else {
    PmStrcpy(file, " "); /* blank filename */
  }

  pnl = l - ar->fname;
  if (pnl > max_path_len) { max_path_len = pnl; }
  if (pnl > 255) {
    printf(_("========== Path name truncated to 255 chars: %s\n"), ar->fname);
    pnl = 255;
    trunc_path++;
  }

  PmStrcpy(spath, ar->fname);
  if (pnl == 0) {
    PmStrcpy(spath, " ");
    printf(_("========== Path length is zero. File=%s\n"), ar->fname);
  }
  if (debug_level >= 10) {
    printf(_("Path: %s\n"), spath.c_str());
    printf(_("File: %s\n"), file.c_str());
  }
}

static bool CopyFileset(FindFilesPacket* ff, JobControlRecord* jcr)
{
  FilesetResource* jcr_fileset = jcr->impl->res.fileset;
  int num;
  bool include = true;

  findFILESET* fileset;
  findFOPTS* current_opts;

  findFILESET* fileset_allocation = (findFILESET*)malloc(sizeof(findFILESET));
  fileset = new (fileset_allocation)(findFILESET);
  ff->fileset = fileset;

  fileset->state = state_none;
  fileset->include_list.init(1, true);
  fileset->exclude_list.init(1, true);

  for (;;) {
    if (include) {
      num = jcr_fileset->include_items.size();
    } else {
      num = jcr_fileset->exclude_items.size();
    }
    for (int i = 0; i < num; i++) {
      IncludeExcludeItem* ie;
      ;
      int j, k;

      if (include) {
        ie = jcr_fileset->include_items[i];
        /* New include */
        findIncludeExcludeItem* incexe_allocation
            = (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
        fileset->incexe = new (incexe_allocation)(findIncludeExcludeItem);
        fileset->include_list.append(fileset->incexe);
      } else {
        ie = jcr_fileset->exclude_items[i];

        /* New exclude */
        findIncludeExcludeItem* incexe_allocation
            = (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
        fileset->incexe = new (incexe_allocation)(findIncludeExcludeItem);
        fileset->exclude_list.append(fileset->incexe);
      }

      for (std::size_t j = 0; j < ie->file_options_list.size(); j++) {
        FileOptions* fo = ie->file_options_list[j];

        findFOPTS* current_opts_allocation
            = (findFOPTS*)malloc(sizeof(findFOPTS));
        current_opts = new (current_opts_allocation)(findFOPTS);

        fileset->incexe->current_opts = current_opts;
        fileset->incexe->opts_list.append(current_opts);

        SetOptions(current_opts, fo->opts);

        for (k = 0; k < fo->regex.size(); k++) {
          current_opts->regex.append(StringToRegex(fo->regex.get(k)));
        }
        for (k = 0; k < fo->regexdir.size(); k++) {
          // fd->fsend("RD %s\n", fo->regexdir.get(k));
          current_opts->regexdir.append(StringToRegex(fo->regexdir.get(k)));
        }
        for (k = 0; k < fo->regexfile.size(); k++) {
          // fd->fsend("RF %s\n", fo->regexfile.get(k));
          current_opts->regexfile.append(StringToRegex(fo->regexfile.get(k)));
        }
        for (k = 0; k < fo->wild.size(); k++) {
          current_opts->wild.append(strdup((const char*)fo->wild.get(k)));
        }
        for (k = 0; k < fo->wilddir.size(); k++) {
          current_opts->wilddir.append(strdup((const char*)fo->wilddir.get(k)));
        }
        for (k = 0; k < fo->wildfile.size(); k++) {
          current_opts->wildfile.append(
              strdup((const char*)fo->wildfile.get(k)));
        }
        for (k = 0; k < fo->wildbase.size(); k++) {
          current_opts->wildbase.append(
              strdup((const char*)fo->wildbase.get(k)));
        }
        for (k = 0; k < fo->fstype.size(); k++) {
          current_opts->fstype.append(strdup((const char*)fo->fstype.get(k)));
        }
        for (k = 0; k < fo->Drivetype.size(); k++) {
          current_opts->Drivetype.append(
              strdup((const char*)fo->Drivetype.get(k)));
        }
      }

      for (j = 0; j < ie->name_list.size(); j++) {
        fileset->incexe->name_list.append(
            new_dlistString(ie->name_list.get(j)));
      }
    }

    if (!include) { /* If we just did excludes */
      break;        /*   all done */
    }

    include = false; /* Now do excludes */
  }

  return true;
}

static void SetOptions(findFOPTS* fo, const char* opts)
{
  int j;
  const char* p;

  for (p = opts; *p; p++) {
    switch (*p) {
      case 'a': /* alway replace */
      case '0': /* no option */
        break;
      case 'e':
        SetBit(FO_EXCLUDE, fo->flags);
        break;
      case 'f':
        SetBit(FO_MULTIFS, fo->flags);
        break;
      case 'h': /* no recursion */
        SetBit(FO_NO_RECURSION, fo->flags);
        break;
      case 'H': /* no hard link handling */
        SetBit(FO_NO_HARDLINK, fo->flags);
        break;
      case 'i':
        SetBit(FO_IGNORECASE, fo->flags);
        break;
      case 'M': /* MD5 */
        SetBit(FO_MD5, fo->flags);
        break;
      case 'n':
        SetBit(FO_NOREPLACE, fo->flags);
        break;
      case 'p': /* use portable data format */
        SetBit(FO_PORTABLE, fo->flags);
        break;
      case 'R': /* Resource forks and Finder Info */
        SetBit(FO_HFSPLUS, fo->flags);
        [[fallthrough]];
      case 'r': /* read fifo */
        SetBit(FO_READFIFO, fo->flags);
        break;
      case 'S':
        switch (*(p + 1)) {
          case ' ':
            /* Old director did not specify SHA variant */
            SetBit(FO_SHA1, fo->flags);
            break;
          case '1':
            SetBit(FO_SHA1, fo->flags);
            p++;
            break;
#ifdef HAVE_SHA2
          case '2':
            SetBit(FO_SHA256, fo->flags);
            p++;
            break;
          case '3':
            SetBit(FO_SHA512, fo->flags);
            p++;
            break;
#endif
          default:
            /* Automatically downgrade to SHA-1 if an unsupported
             * SHA variant is specified */
            SetBit(FO_SHA1, fo->flags);
            p++;
            break;
        }
        break;
      case 's':
        SetBit(FO_SPARSE, fo->flags);
        break;
      case 'm':
        SetBit(FO_MTIMEONLY, fo->flags);
        break;
      case 'k':
        SetBit(FO_KEEPATIME, fo->flags);
        break;
      case 'A':
        SetBit(FO_ACL, fo->flags);
        break;
      case 'V': /* verify options */
        /* Copy Verify Options */
        for (j = 0; *p && *p != ':'; p++) {
          fo->VerifyOpts[j] = *p;
          if (j < (int)sizeof(fo->VerifyOpts) - 1) { j++; }
        }
        fo->VerifyOpts[j] = 0;
        break;
      case 'w':
        SetBit(FO_IF_NEWER, fo->flags);
        break;
      case 'W':
        SetBit(FO_ENHANCEDWILD, fo->flags);
        break;
      case 'Z': /* compression */
        p++;    /* skip Z */
        if (*p >= '0' && *p <= '9') {
          SetBit(FO_COMPRESS, fo->flags);
          fo->Compress_algo = COMPRESS_GZIP;
          fo->Compress_level = *p - '0';
        } else if (*p == 'o') {
          SetBit(FO_COMPRESS, fo->flags);
          fo->Compress_algo = COMPRESS_LZO1X;
          fo->Compress_level = 1; /* not used with LZO */
        }
        Dmsg2(200, "Compression alg=%d level=%d\n", fo->Compress_algo,
              fo->Compress_level);
        break;
      case 'x':
        SetBit(FO_NO_AUTOEXCL, fo->flags);
        break;
      case 'X':
        SetBit(FO_XATTR, fo->flags);
        break;
      default:
        Emsg1(M_ERROR, 0, _("Unknown include/exclude option: %c\n"), *p);
        break;
    }
  }
}
