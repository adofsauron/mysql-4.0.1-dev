
SUBDIRS = bank

ndbtest_PROGRAMS = \
flexBench \
drop_all_tabs \
create_all_tabs \
create_tab \
flexAsynch \
flexBench \
flexHammer \
flexTT \
testBackup \
testBasic \
testBasicAsynch \
testBlobs \
testDataBuffers \
testDict \
testIndex \
testMgm \
testNdbApi \
testNodeRestart \
testOIBasic \
testOperations \
testRestartGci \
testScan \
testScanInterpreter \
testScanPerf \
testSystemRestart \
testTimeout \
testTransactions \
testDeadlock \
test_event ndbapi_slow_select testReadPerf testLcp \
testPartitioning \
testBitfield \
DbCreate DbAsyncGenerator \
test_event_multi_table

#flexTimedAsynch
#testBlobs
#flex_bench_mysql

create_all_tabs_SOURCES = create_all_tabs.cpp
create_tab_SOURCES = create_tab.cpp
drop_all_tabs_SOURCES = drop_all_tabs.cpp
flexAsynch_SOURCES = flexAsynch.cpp
flexBench_SOURCES = flexBench.cpp
flexHammer_SOURCES = flexHammer.cpp
flexTT_SOURCES = flexTT.cpp
#flexTimedAsynch_SOURCES = flexTimedAsynch.cpp
#flex_bench_mysql_SOURCES = flex_bench_mysql.cpp
testBackup_SOURCES = testBackup.cpp
testBasic_SOURCES = testBasic.cpp
testBasicAsynch_SOURCES = testBasicAsynch.cpp
testBlobs_SOURCES = testBlobs.cpp
testDataBuffers_SOURCES = testDataBuffers.cpp
testDict_SOURCES = testDict.cpp
testIndex_SOURCES = testIndex.cpp
testMgm_SOURCES = testMgm.cpp
testNdbApi_SOURCES = testNdbApi.cpp
testNodeRestart_SOURCES = testNodeRestart.cpp
testOIBasic_SOURCES = testOIBasic.cpp
testOperations_SOURCES = testOperations.cpp
testRestartGci_SOURCES = testRestartGci.cpp
testScan_SOURCES = testScan.cpp ScanFunctions.hpp
testScanInterpreter_SOURCES = testScanInterpreter.cpp ScanFilter.hpp ScanInterpretTest.hpp 
testScanPerf_SOURCES = testScanPerf.cpp
testSystemRestart_SOURCES = testSystemRestart.cpp
testTimeout_SOURCES = testTimeout.cpp
testTransactions_SOURCES = testTransactions.cpp
testDeadlock_SOURCES = testDeadlock.cpp
test_event_SOURCES = test_event.cpp
ndbapi_slow_select_SOURCES = slow_select.cpp
testReadPerf_SOURCES = testReadPerf.cpp
testLcp_SOURCES = testLcp.cpp
testPartitioning_SOURCES = testPartitioning.cpp
testBitfield_SOURCES = testBitfield.cpp
DbCreate_SOURCES = bench/mainPopulate.cpp bench/dbPopulate.cpp bench/userInterface.cpp bench/dbPopulate.h bench/userInterface.h bench/testData.h bench/testDefinitions.h bench/ndb_schema.hpp bench/ndb_error.hpp
DbAsyncGenerator_SOURCES = bench/mainAsyncGenerator.cpp bench/asyncGenerator.cpp bench/ndb_async2.cpp bench/dbGenerator.h bench/macros.h bench/userInterface.h bench/testData.h bench/testDefinitions.h bench/ndb_schema.hpp bench/ndb_error.hpp
test_event_multi_table_SOURCES = test_event_multi_table.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/kernel

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapitest.mk.am

##testDict_INCLUDES = $(INCLUDES) -I$(top_srcdir)/ndb/include/kernel
##testIndex_INCLUDES = $(INCLUDES) -I$(top_srcdir)/ndb/include/kernel
##testSystemRestart_INCLUDES = $(INCLUDES) -I$(top_srcdir)/ndb/include/kernel
##testTransactions_INCLUDES = $(INCLUDES) -I$(top_srcdir)/ndb/include/kernel
testBackup_LDADD = $(LDADD) bank/libbank.a

# Don't update the files from bitkeeper
%::SCCS/s.%



windoze-dsp: flexBench.dsp testBasic.dsp testBlobs.dsp \
             testScan.dsp

flexBench.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ flexBench
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(flexBench_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

testBasic.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ testBasic
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(testBasic_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

testOIBasic.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ testOIBasic
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(testOIBasic_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

testBlobs.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ testBlobs
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(testBlobs_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

testScan.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ testScan
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(testScan_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)
