#!/bin/sh
# Copyright (C) 1997-2003 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file

# This script writes on stdout SQL commands to generate all not
# existing MySQL system tables. It also replaces the help tables with
# new context from the manual (from fill_help_tables.sql).

# $1 - "test" or "real" or "verbose" variant of database
# $2 - path to mysql-database directory
# $3 - hostname  
# $4 - windows option

if test "$1" = ""
then
  echo "
This script writes on stdout SQL commands to generate all not
existing MySQL system tables. It also replaces the help tables with
new context from the manual (from fill_help_tables.sql).

Usage:
  mysql_create_system_tables [test|verbose|real] <path to mysql-database directory> <hostname> <windows option>
"
  exit
fi

mdata=$2
hostname=$3
windows=$4

# Initialize variables
c_d="" i_d=""
c_h="" i_h=""
c_u="" i_u=""
c_f="" i_f=""
c_t="" c_c=""
c_ht=""
c_hc=""
c_hr="" 
c_hk="" 
i_ht=""
c_tzn="" c_tz="" c_tzt="" c_tztt="" c_tzls=""
i_tzn="" i_tz="" i_tzt="" i_tztt="" i_tzls=""
c_p="" c_pp=""

# Check for old tables
if test ! -f $mdata/db.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing db table" 1>&2; 
  fi

  # mysqld --bootstrap wants one command/line
  c_d="$c_d CREATE TABLE db ("
  c_d="$c_d   Host char(60) binary DEFAULT '' NOT NULL,"
  c_d="$c_d   Db char(64) binary DEFAULT '' NOT NULL,"
  c_d="$c_d   User char(16) binary DEFAULT '' NOT NULL,"
  c_d="$c_d   Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_d="$c_d PRIMARY KEY Host (Host,Db,User),"
  c_d="$c_d KEY User (User)"
  c_d="$c_d ) engine=MyISAM"
  c_d="$c_d CHARACTER SET utf8 COLLATE utf8_bin"
  c_d="$c_d comment='Database privileges';"
  
  i_d="INSERT INTO db VALUES ('%','test','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y','Y','Y','Y','Y','Y','N','N');
  INSERT INTO db VALUES ('%','test\_%','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y','Y','Y','Y','Y','Y','N','N');"
fi

if test ! -f $mdata/host.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing host table" 1>&2;
  fi

  c_h="$c_h CREATE TABLE host ("
  c_h="$c_h  Host char(60) binary DEFAULT '' NOT NULL,"
  c_h="$c_h  Db char(64) binary DEFAULT '' NOT NULL,"
  c_h="$c_h  Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_h="$c_h  PRIMARY KEY Host (Host,Db)"
  c_h="$c_h ) engine=MyISAM"
  c_h="$c_h CHARACTER SET utf8 COLLATE utf8_bin"
  c_h="$c_h comment='Host privileges;  Merged with database privileges';"
fi

if test ! -f $mdata/user.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing user table" 1>&2;
  fi

  c_u="$c_u CREATE TABLE user ("
  c_u="$c_u   Host char(60) binary DEFAULT '' NOT NULL,"
  c_u="$c_u   User char(16) binary DEFAULT '' NOT NULL,"
  c_u="$c_u   Password char(41) binary DEFAULT '' NOT NULL,"
  c_u="$c_u   Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Reload_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Shutdown_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Process_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   File_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Show_db_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Super_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Repl_slave_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Repl_client_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_user_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_u="$c_u   ssl_type enum('','ANY','X509', 'SPECIFIED') COLLATE utf8_general_ci DEFAULT '' NOT NULL,"
  c_u="$c_u   ssl_cipher BLOB NOT NULL,"
  c_u="$c_u   x509_issuer BLOB NOT NULL,"
  c_u="$c_u   x509_subject BLOB NOT NULL,"
  c_u="$c_u   max_questions int(11) unsigned DEFAULT 0  NOT NULL,"
  c_u="$c_u   max_updates int(11) unsigned DEFAULT 0  NOT NULL,"
  c_u="$c_u   max_connections int(11) unsigned DEFAULT 0  NOT NULL,"
  c_u="$c_u   max_user_connections int(11) unsigned DEFAULT 0  NOT NULL,"
  c_u="$c_u   PRIMARY KEY Host (Host,User)"
  c_u="$c_u ) engine=MyISAM"
  c_u="$c_u CHARACTER SET utf8 COLLATE utf8_bin"
  c_u="$c_u comment='Users and global privileges';"

  if test "$1" = "test" 
  then
    i_u="INSERT INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0);
    INSERT INTO user VALUES ('$hostname','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0);
    REPLACE INTO user VALUES ('127.0.0.1','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0);
    INSERT INTO user (host,user) values ('localhost','');
    INSERT INTO user (host,user) values ('$hostname','');"
  else
    i_u="INSERT INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0);"
    if test "$windows" = "0"
    then
      i_u="$i_u
           INSERT INTO user VALUES ('$hostname','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0);
           INSERT INTO user (host,user) values ('$hostname','');
           INSERT INTO user (host,user) values ('localhost','');"
    else
      i_u="$i_u
	   INSERT INTO user VALUES ('localhost','','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);"
    fi
  fi 
fi

if test ! -f $mdata/func.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing func table" 1>&2;
  fi

  c_f="$c_f CREATE TABLE func ("
  c_f="$c_f   name char(64) binary DEFAULT '' NOT NULL,"
  c_f="$c_f   ret tinyint(1) DEFAULT '0' NOT NULL,"
  c_f="$c_f   dl char(128) DEFAULT '' NOT NULL,"
  c_f="$c_f   type enum ('function','aggregate') COLLATE utf8_general_ci NOT NULL,"
  c_f="$c_f   PRIMARY KEY (name)"
  c_f="$c_f ) engine=MyISAM"
  c_f="$c_f CHARACTER SET utf8 COLLATE utf8_bin"
  c_f="$c_f   comment='User defined functions';"
fi

if test ! -f $mdata/tables_priv.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing tables_priv table" 1>&2;
  fi

  c_t="$c_t CREATE TABLE tables_priv ("
  c_t="$c_t   Host char(60) binary DEFAULT '' NOT NULL,"
  c_t="$c_t   Db char(64) binary DEFAULT '' NOT NULL,"
  c_t="$c_t   User char(16) binary DEFAULT '' NOT NULL,"
  c_t="$c_t   Table_name char(64) binary DEFAULT '' NOT NULL,"
  c_t="$c_t   Grantor char(77) DEFAULT '' NOT NULL,"
  c_t="$c_t   Timestamp timestamp(14),"
  c_t="$c_t   Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter','Create View','Show view') COLLATE utf8_general_ci DEFAULT '' NOT NULL,"
  c_t="$c_t   Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL,"
  c_t="$c_t   PRIMARY KEY (Host,Db,User,Table_name),"
  c_t="$c_t   KEY Grantor (Grantor)"
  c_t="$c_t ) engine=MyISAM"
  c_t="$c_t CHARACTER SET utf8 COLLATE utf8_bin"
  c_t="$c_t   comment='Table privileges';"
fi

if test ! -f $mdata/columns_priv.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing columns_priv table" 1>&2;
  fi

  c_c="$c_c CREATE TABLE columns_priv ("
  c_c="$c_c   Host char(60) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Db char(64) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   User char(16) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Table_name char(64) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Column_name char(64) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Timestamp timestamp(14),"
  c_c="$c_c   Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL,"
  c_c="$c_c   PRIMARY KEY (Host,Db,User,Table_name,Column_name)"
  c_c="$c_c ) engine=MyISAM"
  c_c="$c_c CHARACTER SET utf8 COLLATE utf8_bin"
  c_c="$c_c   comment='Column privileges';"
fi

if test ! -f $mdata/procs_priv.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing procs_priv table" 1>&2;
  fi

  c_pp="$c_pp CREATE TABLE procs_priv ("
  c_pp="$c_pp   Host char(60) binary DEFAULT '' NOT NULL,"
  c_pp="$c_pp   Db char(64) binary DEFAULT '' NOT NULL,"
  c_pp="$c_pp   User char(16) binary DEFAULT '' NOT NULL,"
  c_pp="$c_pp   Routine_name char(64) binary DEFAULT '' NOT NULL,"
  c_pp="$c_pp   Routine_type enum('FUNCTION','PROCEDURE') NOT NULL,"
  c_pp="$c_pp   Grantor char(77) DEFAULT '' NOT NULL,"
  c_pp="$c_pp   Proc_priv set('Execute','Alter Routine','Grant') COLLATE utf8_general_ci DEFAULT '' NOT NULL,"
  c_pp="$c_pp   Timestamp timestamp(14),"
  c_pp="$c_pp   PRIMARY KEY (Host,Db,User,Routine_name,Routine_type),"
  c_pp="$c_pp   KEY Grantor (Grantor)"
  c_pp="$c_pp ) engine=MyISAM"
  c_pp="$c_pp CHARACTER SET utf8 COLLATE utf8_bin"
  c_pp="$c_pp   comment='Procedure privileges';"
fi

if test ! -f $mdata/help_topic.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing help_topic table" 1>&2;
  fi

  c_ht="$c_ht CREATE TABLE help_topic ("
  c_ht="$c_ht   help_topic_id    int unsigned not null,"
  c_ht="$c_ht   name             char(64) not null,"
  c_ht="$c_ht   help_category_id smallint unsigned not null,"
  c_ht="$c_ht   description      text not null,"
  c_ht="$c_ht   example          text not null,"
  c_ht="$c_ht   url              char(128) not null,"
  c_ht="$c_ht   primary key      (help_topic_id),"
  c_ht="$c_ht   unique index     (name)"
  c_ht="$c_ht ) engine=MyISAM"
  c_ht="$c_ht CHARACTER SET utf8"
  c_ht="$c_ht   comment='help topics';"
fi

old_categories="yes"
		    
if test ! -f $mdata/help_category.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing help_category table" 1>&2;
  fi
  
  c_hc="$c_hc CREATE TABLE help_category ("
  c_hc="$c_hc   help_category_id   smallint unsigned not null,"
  c_hc="$c_hc   name               char(64) not null,"
  c_hc="$c_hc   parent_category_id smallint unsigned null,"
  c_hc="$c_hc   url                char(128) not null,"
  c_hc="$c_hc   primary key        (help_category_id),"
  c_hc="$c_hc   unique index       (name)"
  c_hc="$c_hc ) engine=MyISAM"
  c_hc="$c_hc CHARACTER SET utf8"
  c_hc="$c_hc   comment='help categories';"
fi

if test ! -f $mdata/help_keyword.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing help_keyword table" 1>&2;
  fi

  c_hk="$c_hk CREATE TABLE help_keyword ("
  c_hk="$c_hk   help_keyword_id  int unsigned not null,"
  c_hk="$c_hk   name             char(64) not null,"
  c_hk="$c_hk   primary key      (help_keyword_id),"
  c_hk="$c_hk   unique index     (name)"
  c_hk="$c_hk ) engine=MyISAM"
  c_hk="$c_hk CHARACTER SET utf8"
  c_hk="$c_hk   comment='help keywords';"
fi
				    
if test ! -f $mdata/help_relation.frm
then
  if test "$1" = "verbose" ; then
   echo "Preparing help_relation table" 1>&2;
  fi

  c_hr="$c_hr CREATE TABLE help_relation ("
  c_hr="$c_hr   help_topic_id    int unsigned not null references help_topic,"
  c_hr="$c_hr   help_keyword_id  int unsigned not null references help_keyword,"
  c_hr="$c_hr   primary key      (help_keyword_id, help_topic_id)"
  c_hr="$c_hr ) engine=MyISAM"
  c_hr="$c_hr CHARACTER SET utf8"
  c_hr="$c_hr   comment='keyword-topic relation';"
fi

if test ! -f $mdata/time_zone_name.frm
then
  if test "$1" = "verbose" ; then
   echo "Preparing time_zone_name table" 1>&2;
  fi

  c_tzn="$c_tzn CREATE TABLE time_zone_name ("
  c_tzn="$c_tzn   Name char(64) NOT NULL,"
  c_tzn="$c_tzn   Time_zone_id int unsigned NOT NULL,"
  c_tzn="$c_tzn   PRIMARY KEY Name (Name)"
  c_tzn="$c_tzn ) engine=MyISAM CHARACTER SET utf8"
  c_tzn="$c_tzn   comment='Time zone names';"
  
  if test "$1" = "test" 
  then
    i_tzn="$i_tzn INSERT INTO time_zone_name (Name, Time_Zone_id) VALUES"
    i_tzn="$i_tzn   ('MET', 1), ('UTC', 2), ('Universal', 2), "
    i_tzn="$i_tzn   ('Europe/Moscow',3), ('leap/Europe/Moscow',4), "
    i_tzn="$i_tzn   ('Japan', 5);"
  fi
fi

if test ! -f $mdata/time_zone.frm
then
  if test "$1" = "verbose" ; then
   echo "Preparing time_zone table" 1>&2;
  fi

  c_tz="$c_tz CREATE TABLE time_zone ("
  c_tz="$c_tz   Time_zone_id int unsigned NOT NULL auto_increment,"
  c_tz="$c_tz   Use_leap_seconds enum('Y','N') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,"
  c_tz="$c_tz   PRIMARY KEY TzId (Time_zone_id)"
  c_tz="$c_tz ) engine=MyISAM CHARACTER SET utf8"
  c_tz="$c_tz   comment='Time zones';"
  
  if test "$1" = "test" 
  then
    i_tz="$i_tz INSERT INTO time_zone (Time_zone_id, Use_leap_seconds)" 
    i_tz="$i_tz   VALUES (1,'N'), (2,'N'), (3,'N'), (4,'Y'), (5,'N');"
  fi
fi

if test ! -f $mdata/time_zone_transition.frm
then
  if test "$1" = "verbose" ; then
   echo "Preparing time_zone_transition table" 1>&2;
  fi

  c_tzt="$c_tzt CREATE TABLE time_zone_transition ("
  c_tzt="$c_tzt   Time_zone_id int unsigned NOT NULL,"
  c_tzt="$c_tzt   Transition_time bigint signed NOT NULL,"
  c_tzt="$c_tzt   Transition_type_id int unsigned NOT NULL,"
  c_tzt="$c_tzt   PRIMARY KEY TzIdTranTime (Time_zone_id, Transition_time)"
  c_tzt="$c_tzt ) engine=MyISAM CHARACTER SET utf8"
  c_tzt="$c_tzt   comment='Time zone transitions';"
  
  if test "$1" = "test" 
  then
    i_tzt="$i_tzt INSERT INTO time_zone_transition"
    i_tzt="$i_tzt   (Time_zone_id, Transition_time, Transition_type_id)"
    i_tzt="$i_tzt VALUES"
    i_tzt="$i_tzt   (1, -1693706400, 0) ,(1, -1680483600, 1)"
    i_tzt="$i_tzt  ,(1, -1663455600, 2) ,(1, -1650150000, 3)"
    i_tzt="$i_tzt  ,(1, -1632006000, 2) ,(1, -1618700400, 3)"
    i_tzt="$i_tzt  ,(1, -938905200, 2) ,(1, -857257200, 3)"
    i_tzt="$i_tzt  ,(1, -844556400, 2) ,(1, -828226800, 3)"
    i_tzt="$i_tzt  ,(1, -812502000, 2) ,(1, -796777200, 3)"
    i_tzt="$i_tzt  ,(1, 228877200, 2) ,(1, 243997200, 3)"
    i_tzt="$i_tzt  ,(1, 260326800, 2) ,(1, 276051600, 3)"
    i_tzt="$i_tzt  ,(1, 291776400, 2) ,(1, 307501200, 3)"
    i_tzt="$i_tzt  ,(1, 323830800, 2) ,(1, 338950800, 3)"
    i_tzt="$i_tzt  ,(1, 354675600, 2) ,(1, 370400400, 3)"
    i_tzt="$i_tzt  ,(1, 386125200, 2) ,(1, 401850000, 3)"
    i_tzt="$i_tzt  ,(1, 417574800, 2) ,(1, 433299600, 3)"
    i_tzt="$i_tzt  ,(1, 449024400, 2) ,(1, 465354000, 3)"
    i_tzt="$i_tzt  ,(1, 481078800, 2) ,(1, 496803600, 3)"
    i_tzt="$i_tzt  ,(1, 512528400, 2) ,(1, 528253200, 3)"
    i_tzt="$i_tzt  ,(1, 543978000, 2) ,(1, 559702800, 3)"
    i_tzt="$i_tzt  ,(1, 575427600, 2) ,(1, 591152400, 3)"
    i_tzt="$i_tzt  ,(1, 606877200, 2) ,(1, 622602000, 3)"
    i_tzt="$i_tzt  ,(1, 638326800, 2) ,(1, 654656400, 3)"
    i_tzt="$i_tzt  ,(1, 670381200, 2) ,(1, 686106000, 3)"
    i_tzt="$i_tzt  ,(1, 701830800, 2) ,(1, 717555600, 3)"
    i_tzt="$i_tzt  ,(1, 733280400, 2) ,(1, 749005200, 3)"
    i_tzt="$i_tzt  ,(1, 764730000, 2) ,(1, 780454800, 3)"
    i_tzt="$i_tzt  ,(1, 796179600, 2) ,(1, 811904400, 3)"
    i_tzt="$i_tzt  ,(1, 828234000, 2) ,(1, 846378000, 3)"
    i_tzt="$i_tzt  ,(1, 859683600, 2) ,(1, 877827600, 3)"
    i_tzt="$i_tzt  ,(1, 891133200, 2) ,(1, 909277200, 3)"
    i_tzt="$i_tzt  ,(1, 922582800, 2) ,(1, 941331600, 3)"
    i_tzt="$i_tzt  ,(1, 954032400, 2) ,(1, 972781200, 3)"
    i_tzt="$i_tzt  ,(1, 985482000, 2) ,(1, 1004230800, 3)"
    i_tzt="$i_tzt  ,(1, 1017536400, 2) ,(1, 1035680400, 3)"
    i_tzt="$i_tzt  ,(1, 1048986000, 2) ,(1, 1067130000, 3)"
    i_tzt="$i_tzt  ,(1, 1080435600, 2) ,(1, 1099184400, 3)"
    i_tzt="$i_tzt  ,(1, 1111885200, 2) ,(1, 1130634000, 3)"
    i_tzt="$i_tzt  ,(1, 1143334800, 2) ,(1, 1162083600, 3)"
    i_tzt="$i_tzt  ,(1, 1174784400, 2) ,(1, 1193533200, 3)"
    i_tzt="$i_tzt  ,(1, 1206838800, 2) ,(1, 1224982800, 3)"
    i_tzt="$i_tzt  ,(1, 1238288400, 2) ,(1, 1256432400, 3)"
    i_tzt="$i_tzt  ,(1, 1269738000, 2) ,(1, 1288486800, 3)"
    i_tzt="$i_tzt  ,(1, 1301187600, 2) ,(1, 1319936400, 3)"
    i_tzt="$i_tzt  ,(1, 1332637200, 2) ,(1, 1351386000, 3)"
    i_tzt="$i_tzt  ,(1, 1364691600, 2) ,(1, 1382835600, 3)"
    i_tzt="$i_tzt  ,(1, 1396141200, 2) ,(1, 1414285200, 3)"
    i_tzt="$i_tzt  ,(1, 1427590800, 2) ,(1, 1445734800, 3)"
    i_tzt="$i_tzt  ,(1, 1459040400, 2) ,(1, 1477789200, 3)"
    i_tzt="$i_tzt  ,(1, 1490490000, 2) ,(1, 1509238800, 3)"
    i_tzt="$i_tzt  ,(1, 1521939600, 2) ,(1, 1540688400, 3)"
    i_tzt="$i_tzt  ,(1, 1553994000, 2) ,(1, 1572138000, 3)"
    i_tzt="$i_tzt  ,(1, 1585443600, 2) ,(1, 1603587600, 3)"
    i_tzt="$i_tzt  ,(1, 1616893200, 2) ,(1, 1635642000, 3)"
    i_tzt="$i_tzt  ,(1, 1648342800, 2) ,(1, 1667091600, 3)"
    i_tzt="$i_tzt  ,(1, 1679792400, 2) ,(1, 1698541200, 3)"
    i_tzt="$i_tzt  ,(1, 1711846800, 2) ,(1, 1729990800, 3)"
    i_tzt="$i_tzt  ,(1, 1743296400, 2) ,(1, 1761440400, 3)"
    i_tzt="$i_tzt  ,(1, 1774746000, 2) ,(1, 1792890000, 3)"
    i_tzt="$i_tzt  ,(1, 1806195600, 2) ,(1, 1824944400, 3)"
    i_tzt="$i_tzt  ,(1, 1837645200, 2) ,(1, 1856394000, 3)"
    i_tzt="$i_tzt  ,(1, 1869094800, 2) ,(1, 1887843600, 3)"
    i_tzt="$i_tzt  ,(1, 1901149200, 2) ,(1, 1919293200, 3)"
    i_tzt="$i_tzt  ,(1, 1932598800, 2) ,(1, 1950742800, 3)"
    i_tzt="$i_tzt  ,(1, 1964048400, 2) ,(1, 1982797200, 3)"
    i_tzt="$i_tzt  ,(1, 1995498000, 2) ,(1, 2014246800, 3)"
    i_tzt="$i_tzt  ,(1, 2026947600, 2) ,(1, 2045696400, 3)"
    i_tzt="$i_tzt  ,(1, 2058397200, 2) ,(1, 2077146000, 3)"
    i_tzt="$i_tzt  ,(1, 2090451600, 2) ,(1, 2108595600, 3)"
    i_tzt="$i_tzt  ,(1, 2121901200, 2) ,(1, 2140045200, 3)"
    i_tzt="$i_tzt  ,(3, -1688265000, 2) ,(3, -1656819048, 1)"
    i_tzt="$i_tzt  ,(3, -1641353448, 2) ,(3, -1627965048, 3)"
    i_tzt="$i_tzt  ,(3, -1618716648, 1) ,(3, -1596429048, 3)"
    i_tzt="$i_tzt  ,(3, -1593829848, 5) ,(3, -1589860800, 4)"
    i_tzt="$i_tzt  ,(3, -1542427200, 5) ,(3, -1539493200, 6)"
    i_tzt="$i_tzt  ,(3, -1525323600, 5) ,(3, -1522728000, 4)"
    i_tzt="$i_tzt  ,(3, -1491188400, 7) ,(3, -1247536800, 4)"
    i_tzt="$i_tzt  ,(3, 354920400, 5) ,(3, 370728000, 4)"
    i_tzt="$i_tzt  ,(3, 386456400, 5) ,(3, 402264000, 4)"
    i_tzt="$i_tzt  ,(3, 417992400, 5) ,(3, 433800000, 4)"
    i_tzt="$i_tzt  ,(3, 449614800, 5) ,(3, 465346800, 8)"
    i_tzt="$i_tzt  ,(3, 481071600, 9) ,(3, 496796400, 8)"
    i_tzt="$i_tzt  ,(3, 512521200, 9) ,(3, 528246000, 8)"
    i_tzt="$i_tzt  ,(3, 543970800, 9) ,(3, 559695600, 8)"
    i_tzt="$i_tzt  ,(3, 575420400, 9) ,(3, 591145200, 8)"
    i_tzt="$i_tzt  ,(3, 606870000, 9) ,(3, 622594800, 8)"
    i_tzt="$i_tzt  ,(3, 638319600, 9) ,(3, 654649200, 8)"
    i_tzt="$i_tzt  ,(3, 670374000, 10) ,(3, 686102400, 11)"
    i_tzt="$i_tzt  ,(3, 695779200, 8) ,(3, 701812800, 5)"
    i_tzt="$i_tzt  ,(3, 717534000, 4) ,(3, 733273200, 9)"
    i_tzt="$i_tzt  ,(3, 748998000, 8) ,(3, 764722800, 9)"
    i_tzt="$i_tzt  ,(3, 780447600, 8) ,(3, 796172400, 9)"
    i_tzt="$i_tzt  ,(3, 811897200, 8) ,(3, 828226800, 9)"
    i_tzt="$i_tzt  ,(3, 846370800, 8) ,(3, 859676400, 9)"
    i_tzt="$i_tzt  ,(3, 877820400, 8) ,(3, 891126000, 9)"
    i_tzt="$i_tzt  ,(3, 909270000, 8) ,(3, 922575600, 9)"
    i_tzt="$i_tzt  ,(3, 941324400, 8) ,(3, 954025200, 9)"
    i_tzt="$i_tzt  ,(3, 972774000, 8) ,(3, 985474800, 9)"
    i_tzt="$i_tzt  ,(3, 1004223600, 8) ,(3, 1017529200, 9)"
    i_tzt="$i_tzt  ,(3, 1035673200, 8) ,(3, 1048978800, 9)"
    i_tzt="$i_tzt  ,(3, 1067122800, 8) ,(3, 1080428400, 9)"
    i_tzt="$i_tzt  ,(3, 1099177200, 8) ,(3, 1111878000, 9)"
    i_tzt="$i_tzt  ,(3, 1130626800, 8) ,(3, 1143327600, 9)"
    i_tzt="$i_tzt  ,(3, 1162076400, 8) ,(3, 1174777200, 9)"
    i_tzt="$i_tzt  ,(3, 1193526000, 8) ,(3, 1206831600, 9)"
    i_tzt="$i_tzt  ,(3, 1224975600, 8) ,(3, 1238281200, 9)"
    i_tzt="$i_tzt  ,(3, 1256425200, 8) ,(3, 1269730800, 9)"
    i_tzt="$i_tzt  ,(3, 1288479600, 8) ,(3, 1301180400, 9)"
    i_tzt="$i_tzt  ,(3, 1319929200, 8) ,(3, 1332630000, 9)"
    i_tzt="$i_tzt  ,(3, 1351378800, 8) ,(3, 1364684400, 9)"
    i_tzt="$i_tzt  ,(3, 1382828400, 8) ,(3, 1396134000, 9)"
    i_tzt="$i_tzt  ,(3, 1414278000, 8) ,(3, 1427583600, 9)"
    i_tzt="$i_tzt  ,(3, 1445727600, 8) ,(3, 1459033200, 9)"
    i_tzt="$i_tzt  ,(3, 1477782000, 8) ,(3, 1490482800, 9)"
    i_tzt="$i_tzt  ,(3, 1509231600, 8) ,(3, 1521932400, 9)"
    i_tzt="$i_tzt  ,(3, 1540681200, 8) ,(3, 1553986800, 9)"
    i_tzt="$i_tzt  ,(3, 1572130800, 8) ,(3, 1585436400, 9)"
    i_tzt="$i_tzt  ,(3, 1603580400, 8) ,(3, 1616886000, 9)"
    i_tzt="$i_tzt  ,(3, 1635634800, 8) ,(3, 1648335600, 9)"
    i_tzt="$i_tzt  ,(3, 1667084400, 8) ,(3, 1679785200, 9)"
    i_tzt="$i_tzt  ,(3, 1698534000, 8) ,(3, 1711839600, 9)"
    i_tzt="$i_tzt  ,(3, 1729983600, 8) ,(3, 1743289200, 9)"
    i_tzt="$i_tzt  ,(3, 1761433200, 8) ,(3, 1774738800, 9)"
    i_tzt="$i_tzt  ,(3, 1792882800, 8) ,(3, 1806188400, 9)"
    i_tzt="$i_tzt  ,(3, 1824937200, 8) ,(3, 1837638000, 9)"
    i_tzt="$i_tzt  ,(3, 1856386800, 8) ,(3, 1869087600, 9)"
    i_tzt="$i_tzt  ,(3, 1887836400, 8) ,(3, 1901142000, 9)"
    i_tzt="$i_tzt  ,(3, 1919286000, 8) ,(3, 1932591600, 9)"
    i_tzt="$i_tzt  ,(3, 1950735600, 8) ,(3, 1964041200, 9)"
    i_tzt="$i_tzt  ,(3, 1982790000, 8) ,(3, 1995490800, 9)"
    i_tzt="$i_tzt  ,(3, 2014239600, 8) ,(3, 2026940400, 9)"
    i_tzt="$i_tzt  ,(3, 2045689200, 8) ,(3, 2058390000, 9)"
    i_tzt="$i_tzt  ,(3, 2077138800, 8) ,(3, 2090444400, 9)"
    i_tzt="$i_tzt  ,(3, 2108588400, 8) ,(3, 2121894000, 9)"
    i_tzt="$i_tzt  ,(3, 2140038000, 8)"
    i_tzt="$i_tzt  ,(4, -1688265000, 2) ,(4, -1656819048, 1)"
    i_tzt="$i_tzt  ,(4, -1641353448, 2) ,(4, -1627965048, 3)"
    i_tzt="$i_tzt  ,(4, -1618716648, 1) ,(4, -1596429048, 3)"
    i_tzt="$i_tzt  ,(4, -1593829848, 5) ,(4, -1589860800, 4)"
    i_tzt="$i_tzt  ,(4, -1542427200, 5) ,(4, -1539493200, 6)"
    i_tzt="$i_tzt  ,(4, -1525323600, 5) ,(4, -1522728000, 4)"
    i_tzt="$i_tzt  ,(4, -1491188400, 7) ,(4, -1247536800, 4)"
    i_tzt="$i_tzt  ,(4, 354920409, 5) ,(4, 370728010, 4)"
    i_tzt="$i_tzt  ,(4, 386456410, 5) ,(4, 402264011, 4)"
    i_tzt="$i_tzt  ,(4, 417992411, 5) ,(4, 433800012, 4)"
    i_tzt="$i_tzt  ,(4, 449614812, 5) ,(4, 465346812, 8)"
    i_tzt="$i_tzt  ,(4, 481071612, 9) ,(4, 496796413, 8)"
    i_tzt="$i_tzt  ,(4, 512521213, 9) ,(4, 528246013, 8)"
    i_tzt="$i_tzt  ,(4, 543970813, 9) ,(4, 559695613, 8)"
    i_tzt="$i_tzt  ,(4, 575420414, 9) ,(4, 591145214, 8)"
    i_tzt="$i_tzt  ,(4, 606870014, 9) ,(4, 622594814, 8)"
    i_tzt="$i_tzt  ,(4, 638319615, 9) ,(4, 654649215, 8)"
    i_tzt="$i_tzt  ,(4, 670374016, 10) ,(4, 686102416, 11)"
    i_tzt="$i_tzt  ,(4, 695779216, 8) ,(4, 701812816, 5)"
    i_tzt="$i_tzt  ,(4, 717534017, 4) ,(4, 733273217, 9)"
    i_tzt="$i_tzt  ,(4, 748998018, 8) ,(4, 764722818, 9)"
    i_tzt="$i_tzt  ,(4, 780447619, 8) ,(4, 796172419, 9)"
    i_tzt="$i_tzt  ,(4, 811897219, 8) ,(4, 828226820, 9)"
    i_tzt="$i_tzt  ,(4, 846370820, 8) ,(4, 859676420, 9)"
    i_tzt="$i_tzt  ,(4, 877820421, 8) ,(4, 891126021, 9)"
    i_tzt="$i_tzt  ,(4, 909270021, 8) ,(4, 922575622, 9)"
    i_tzt="$i_tzt  ,(4, 941324422, 8) ,(4, 954025222, 9)"
    i_tzt="$i_tzt  ,(4, 972774022, 8) ,(4, 985474822, 9)"
    i_tzt="$i_tzt  ,(4, 1004223622, 8) ,(4, 1017529222, 9)"
    i_tzt="$i_tzt  ,(4, 1035673222, 8) ,(4, 1048978822, 9)"
    i_tzt="$i_tzt  ,(4, 1067122822, 8) ,(4, 1080428422, 9)"
    i_tzt="$i_tzt  ,(4, 1099177222, 8) ,(4, 1111878022, 9)"
    i_tzt="$i_tzt  ,(4, 1130626822, 8) ,(4, 1143327622, 9)"
    i_tzt="$i_tzt  ,(4, 1162076422, 8) ,(4, 1174777222, 9)"
    i_tzt="$i_tzt  ,(4, 1193526022, 8) ,(4, 1206831622, 9)"
    i_tzt="$i_tzt  ,(4, 1224975622, 8) ,(4, 1238281222, 9)"
    i_tzt="$i_tzt  ,(4, 1256425222, 8) ,(4, 1269730822, 9)"
    i_tzt="$i_tzt  ,(4, 1288479622, 8) ,(4, 1301180422, 9)"
    i_tzt="$i_tzt  ,(4, 1319929222, 8) ,(4, 1332630022, 9)"
    i_tzt="$i_tzt  ,(4, 1351378822, 8) ,(4, 1364684422, 9)"
    i_tzt="$i_tzt  ,(4, 1382828422, 8) ,(4, 1396134022, 9)"
    i_tzt="$i_tzt  ,(4, 1414278022, 8) ,(4, 1427583622, 9)"
    i_tzt="$i_tzt  ,(4, 1445727622, 8) ,(4, 1459033222, 9)"
    i_tzt="$i_tzt  ,(4, 1477782022, 8) ,(4, 1490482822, 9)"
    i_tzt="$i_tzt  ,(4, 1509231622, 8) ,(4, 1521932422, 9)"
    i_tzt="$i_tzt  ,(4, 1540681222, 8) ,(4, 1553986822, 9)"
    i_tzt="$i_tzt  ,(4, 1572130822, 8) ,(4, 1585436422, 9)"
    i_tzt="$i_tzt  ,(4, 1603580422, 8) ,(4, 1616886022, 9)"
    i_tzt="$i_tzt  ,(4, 1635634822, 8) ,(4, 1648335622, 9)"
    i_tzt="$i_tzt  ,(4, 1667084422, 8) ,(4, 1679785222, 9)"
    i_tzt="$i_tzt  ,(4, 1698534022, 8) ,(4, 1711839622, 9)"
    i_tzt="$i_tzt  ,(4, 1729983622, 8) ,(4, 1743289222, 9)"
    i_tzt="$i_tzt  ,(4, 1761433222, 8) ,(4, 1774738822, 9)"
    i_tzt="$i_tzt  ,(4, 1792882822, 8) ,(4, 1806188422, 9)"
    i_tzt="$i_tzt  ,(4, 1824937222, 8) ,(4, 1837638022, 9)"
    i_tzt="$i_tzt  ,(4, 1856386822, 8) ,(4, 1869087622, 9)"
    i_tzt="$i_tzt  ,(4, 1887836422, 8) ,(4, 1901142022, 9)"
    i_tzt="$i_tzt  ,(4, 1919286022, 8) ,(4, 1932591622, 9)"
    i_tzt="$i_tzt  ,(4, 1950735622, 8) ,(4, 1964041222, 9)"
    i_tzt="$i_tzt  ,(4, 1982790022, 8) ,(4, 1995490822, 9)"
    i_tzt="$i_tzt  ,(4, 2014239622, 8) ,(4, 2026940422, 9)"
    i_tzt="$i_tzt  ,(4, 2045689222, 8) ,(4, 2058390022, 9)"
    i_tzt="$i_tzt  ,(4, 2077138822, 8) ,(4, 2090444422, 9)"
    i_tzt="$i_tzt  ,(4, 2108588422, 8) ,(4, 2121894022, 9)"
    i_tzt="$i_tzt  ,(4, 2140038022, 8)"
    i_tzt="$i_tzt  ,(5, -1009875600, 1);"
  fi
fi

if test ! -f $mdata/time_zone_transition_type.frm
then
  if test "$1" = "verbose" ; then
   echo "Preparing time_zone_transition_type table" 1>&2;
  fi

  c_tztt="$c_tztt CREATE TABLE time_zone_transition_type ("
  c_tztt="$c_tztt   Time_zone_id int unsigned NOT NULL,"
  c_tztt="$c_tztt   Transition_type_id int unsigned NOT NULL,"
  c_tztt="$c_tztt   Offset int signed DEFAULT 0 NOT NULL,"
  c_tztt="$c_tztt   Is_DST tinyint unsigned DEFAULT 0 NOT NULL,"
  c_tztt="$c_tztt   Abbreviation char(8) DEFAULT '' NOT NULL,"
  c_tztt="$c_tztt   PRIMARY KEY TzIdTrTId (Time_zone_id, Transition_type_id)"
  c_tztt="$c_tztt ) engine=MyISAM CHARACTER SET utf8"
  c_tztt="$c_tztt   comment='Time zone transition types';"
  
  if test "$1" = "test" 
  then
    i_tztt="$i_tztt INSERT INTO time_zone_transition_type (Time_zone_id,"
    i_tztt="$i_tztt  Transition_type_id, Offset, Is_DST, Abbreviation) VALUES"
    i_tztt="$i_tztt   (1, 0, 7200, 1, 'MEST') ,(1, 1, 3600, 0, 'MET')"
    i_tztt="$i_tztt  ,(1, 2, 7200, 1, 'MEST') ,(1, 3, 3600, 0, 'MET')"
    i_tztt="$i_tztt  ,(2, 0, 0, 0, 'UTC')"
    i_tztt="$i_tztt  ,(3, 0, 9000, 0, 'MMT') ,(3, 1, 12648, 1, 'MST')"
    i_tztt="$i_tztt  ,(3, 2, 9048, 0, 'MMT') ,(3, 3, 16248, 1, 'MDST')"
    i_tztt="$i_tztt  ,(3, 4, 10800, 0, 'MSK') ,(3, 5, 14400, 1, 'MSD')"
    i_tztt="$i_tztt  ,(3, 6, 18000, 1, 'MSD') ,(3, 7, 7200, 0, 'EET')"
    i_tztt="$i_tztt  ,(3, 8, 10800, 0, 'MSK') ,(3, 9, 14400, 1, 'MSD')"
    i_tztt="$i_tztt  ,(3, 10, 10800, 1, 'EEST') ,(3, 11, 7200, 0, 'EET')"
    i_tztt="$i_tztt  ,(4, 0, 9000, 0, 'MMT') ,(4, 1, 12648, 1, 'MST')"
    i_tztt="$i_tztt  ,(4, 2, 9048, 0, 'MMT') ,(4, 3, 16248, 1, 'MDST')"
    i_tztt="$i_tztt  ,(4, 4, 10800, 0, 'MSK') ,(4, 5, 14400, 1, 'MSD')"
    i_tztt="$i_tztt  ,(4, 6, 18000, 1, 'MSD') ,(4, 7, 7200, 0, 'EET')"
    i_tztt="$i_tztt  ,(4, 8, 10800, 0, 'MSK') ,(4, 9, 14400, 1, 'MSD')"
    i_tztt="$i_tztt  ,(4, 10, 10800, 1, 'EEST') ,(4, 11, 7200, 0, 'EET')"
    i_tztt="$i_tztt  ,(5, 0, 32400, 0, 'CJT') ,(5, 1, 32400, 0, 'JST');"
  fi
fi

if test ! -f $mdata/time_zone_leap_second.frm
then
  if test "$1" = "verbose" ; then
   echo "Preparing time_zone_leap_second table" 1>&2;
  fi

  c_tzls="$c_tzls CREATE TABLE time_zone_leap_second ("
  c_tzls="$c_tzls   Transition_time bigint signed NOT NULL,"
  c_tzls="$c_tzls   Correction int signed NOT NULL,"
  c_tzls="$c_tzls   PRIMARY KEY TranTime (Transition_time)"
  c_tzls="$c_tzls ) engine=MyISAM CHARACTER SET utf8"
  c_tzls="$c_tzls   comment='Leap seconds information for time zones';"
  
  if test "$1" = "test" 
  then
    i_tzls="$i_tzls INSERT INTO time_zone_leap_second "
    i_tzls="$i_tzls  (Transition_time, Correction) VALUES "
    i_tzls="$i_tzls  (78796800, 1) ,(94694401, 2) ,(126230402, 3)"
    i_tzls="$i_tzls ,(157766403, 4) ,(189302404, 5) ,(220924805, 6)"
    i_tzls="$i_tzls ,(252460806, 7) ,(283996807, 8) ,(315532808, 9)"
    i_tzls="$i_tzls ,(362793609, 10) ,(394329610, 11) ,(425865611, 12)"
    i_tzls="$i_tzls ,(489024012, 13) ,(567993613, 14) ,(631152014, 15)"
    i_tzls="$i_tzls ,(662688015, 16) ,(709948816, 17) ,(741484817, 18)"
    i_tzls="$i_tzls ,(773020818, 19) ,(820454419, 20) ,(867715220, 21)"
    i_tzls="$i_tzls ,(915148821, 22);"
  fi
fi

if test ! -f $mdata/proc.frm
then
  c_p="$c_p CREATE TABLE proc ("
  c_p="$c_p   db                char(64) binary DEFAULT '' NOT NULL,"
  c_p="$c_p   name              char(64) DEFAULT '' NOT NULL,"
  c_p="$c_p   type              enum('FUNCTION','PROCEDURE') NOT NULL,"
  c_p="$c_p   specific_name     char(64) DEFAULT '' NOT NULL,"
  c_p="$c_p   language          enum('SQL') DEFAULT 'SQL' NOT NULL,"
  c_p="$c_p   sql_data_access   enum('CONTAINS_SQL',"
  c_p="$c_p			     'NO_SQL',"
  c_p="$c_p			     'READS_SQL_DATA',"
  c_p="$c_p			     'MODIFIES_SQL_DATA'"
  c_p="$c_p                     ) DEFAULT 'CONTAINS_SQL' NOT NULL,"
  c_p="$c_p   is_deterministic  enum('YES','NO') DEFAULT 'NO' NOT NULL,"
  c_p="$c_p   security_type     enum('INVOKER','DEFINER') DEFAULT 'DEFINER' NOT NULL,"
  c_p="$c_p   param_list        blob DEFAULT '' NOT NULL,"
  c_p="$c_p   returns           char(64) DEFAULT '' NOT NULL,"
  c_p="$c_p   body              blob DEFAULT '' NOT NULL,"
  c_p="$c_p   definer           char(77) binary DEFAULT '' NOT NULL,"
  c_p="$c_p   created           timestamp,"
  c_p="$c_p   modified          timestamp,"
  c_p="$c_p   sql_mode          set("
  c_p="$c_p                         'REAL_AS_FLOAT',"
  c_p="$c_p                         'PIPES_AS_CONCAT',"
  c_p="$c_p                         'ANSI_QUOTES',"
  c_p="$c_p                         'IGNORE_SPACE',"
  c_p="$c_p                         'NOT_USED',"
  c_p="$c_p                         'ONLY_FULL_GROUP_BY',"
  c_p="$c_p                         'NO_UNSIGNED_SUBTRACTION',"
  c_p="$c_p                         'NO_DIR_IN_CREATE',"
  c_p="$c_p                         'POSTGRESQL',"
  c_p="$c_p                         'ORACLE',"
  c_p="$c_p                         'MSSQL',"
  c_p="$c_p                         'DB2',"
  c_p="$c_p                         'MAXDB',"
  c_p="$c_p                         'NO_KEY_OPTIONS',"
  c_p="$c_p                         'NO_TABLE_OPTIONS',"
  c_p="$c_p                         'NO_FIELD_OPTIONS',"
  c_p="$c_p                         'MYSQL323',"
  c_p="$c_p                         'MYSQL40',"
  c_p="$c_p                         'ANSI',"
  c_p="$c_p                         'NO_AUTO_VALUE_ON_ZERO',"
  c_p="$c_p                         'NO_BACKSLASH_ESCAPES',"
  c_p="$c_p                         'STRICT_TRANS_TABLES',"
  c_p="$c_p                         'STRICT_ALL_TABLES',"
  c_p="$c_p                         'NO_ZERO_IN_DATE',"
  c_p="$c_p                         'NO_ZERO_DATE',"
  c_p="$c_p                         'INVALID_DATES',"
  c_p="$c_p                         'ERROR_FOR_DIVISION_BY_ZERO',"
  c_p="$c_p                         'TRADITIONAL',"
  c_p="$c_p                         'NO_AUTO_CREATE_USER',"
  c_p="$c_p                         'HIGH_NOT_PRECEDENCE'"
  c_p="$c_p                     ) DEFAULT 0 NOT NULL,"
  c_p="$c_p   comment           char(64) binary DEFAULT '' NOT NULL,"
  c_p="$c_p   PRIMARY KEY (db,name,type)"
  c_p="$c_p ) comment='Stored Procedures';"
fi

cat << END_OF_DATA
use mysql;
set table_type=myisam;
$c_d
$i_d

$c_h
$i_h

$c_u
$i_u

$c_f
$i_f

$c_t
$c_c

$c_ht
$c_hc
$c_hr
$c_hk

$c_tzn
$i_tzn
$c_tz
$i_tz
$c_tzt
$i_tzt
$c_tztt
$i_tztt
$c_tzls
$i_tzls

$c_p
$c_pp

END_OF_DATA

