/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>

#include <NdbOut.hpp>

#include <NdbApi.hpp>
#include <NdbMain.h>
#include <NDBT.hpp> 
#include <NdbSleep.h>
#include <getarg.h>

#include <HugoTransactions.hpp>

int main(int argc, const char** argv){
  ndb_init();

  int _records = 0;
  int _loops = 1;
  int _parallelism = 1;
  int _ver2 = 0;
  const char* _tabname = NULL, *db = 0;
  int _help = 0;
  
  struct getargs args[] = {
    { "loops", 'l', arg_integer, &_loops, "number of times to run this program(0=infinite loop)", "loops" },
    { "parallelism", 'p', arg_integer, &_parallelism, "parallelism(1-240)", "para" },
    { "records", 'r', arg_integer, &_records, "Number of records", "recs" },
    { "ver2", '2', arg_flag, &_ver2, "Use version 2 of scanUpdateRecords", "" },
    { "ver2", '1', arg_negative_flag, &_ver2, "Use version 1 of scanUpdateRecords (default)", "" },
    { "usage", '?', arg_flag, &_help, "Print help", "" },
    { "database", 'd', arg_string, &db, "Database", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program will scan update all records in one table in Ndb\n";
  
  if(getarg(args, num_args, argc, argv, &optind) ||
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];

  // Connect to Ndb
  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb MyNdb( &con, db ? db : "TEST_DB" );

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
   
  // Check if table exists in db
  const NdbDictionary::Table * pTab = NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
  if(pTab == NULL){
    ndbout << " Table " << _tabname << " does not exist!" << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  HugoTransactions hugoTrans(*pTab);
  int i = 0;
  int res = NDBT_FAILED;
  while (i<_loops || _loops==0) {
    ndbout << i << ": ";
    if (_ver2 == 0){
      res = hugoTrans.scanUpdateRecords(&MyNdb, 
				      _records,
				      0, 
				      _parallelism);
    } else{
      res = hugoTrans.scanUpdateRecords2(&MyNdb, 
				       _records,
				       0, 
				       _parallelism);
    }
    if (res != NDBT_OK ){
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    i++;
    //NdbSleep_MilliSleep(300);
  }

  return NDBT_ProgramExit(NDBT_OK);
}
