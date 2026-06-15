/*
This file is part of Explorer Plugin for Notepad++
Copyright (C)2006 Jens Lorenz <jens.plugin.npp@gmx.de>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#pragma once

#define IDC_UPDOWN							101
#define IDC_LEFTRIGHT						102
#define IDI_PARENTFOLDER					103
#define IDI_FOLDER                          104
#define IDI_FILE                            105
#define IDI_WEB                             106
#define IDI_SESSION                         107
#define IDI_GROUP							108
#define IDI_GROUPOPEN						109
#define IDI_HEART                           110
#define IDI_EXPLORE							111
#define IDI_WARN_SESSION					112
#define IDI_MISSING_FILE					113
#define IDI_TB_FLUENT_EXPLORER				114
#define IDI_TB_FLUENT_EXPLORER_DARKMODE		115
#define IDI_TB_FLUENT_FAVES					116
#define IDI_TB_FLUENT_FAVES_DARKMODE		117


/* Toolbar icons */
#define	IDB_EX_PREV							1050
#define	IDB_EX_NEXT							1051
#define	IDB_EX_FILENEW						1052
#define IDB_EX_FOLDERNEW                    1053
#define	IDB_EX_FIND							1054
#define IDB_EX_TERMINAL						1055
#define	IDB_EX_FOLDERGO						1056
#define	IDB_EX_UPDATE						1057
#define IDB_EX_LINKNEWFILE                  1058
#define IDB_EX_LINKNEWFOLDER                1059
#define IDB_EX_LINKNEW                      1060
#define IDB_EX_LINKEDIT                     1061
#define IDB_EX_LINKDELETE                   1062
#define IDB_TB_EXPLORER						1063
#define IDB_TB_FAVES						1064
#define IDB_EX_FOLDERUSER					1065
#define IDB_EX_WORKSPACE                    1066

#define		IDM_TOOLBAR		2000
	#define	IDM_EX_FAVORITES				(IDM_TOOLBAR + 1)
	#define	IDM_EX_PREV						(IDM_TOOLBAR + 2)
	#define	IDM_EX_NEXT						(IDM_TOOLBAR + 3)
	#define	IDM_EX_FILE_NEW					(IDM_TOOLBAR + 4)
	#define	IDM_EX_FOLDER_NEW				(IDM_TOOLBAR + 5)
	#define	IDM_EX_SEARCH_FIND				(IDM_TOOLBAR + 6)
	#define	IDM_EX_TERMINAL					(IDM_TOOLBAR + 7)
	#define	IDM_EX_GO_TO_FOLDER				(IDM_TOOLBAR + 8)
	#define	IDM_EX_GO_TO_USER				(IDM_TOOLBAR + 9)
	#define IDM_EX_UPDATE					(IDM_TOOLBAR + 10)

	#define	IDM_EX_EXPLORER					(IDM_TOOLBAR + 11)
	#define	IDM_EX_TOGGLE_WORKSPACE			(IDM_TOOLBAR + 12)
	#define	IDM_EX_LINK_NEW_FILE			(IDM_TOOLBAR + 13)
	#define	IDM_EX_LINK_NEW_FOLDER			(IDM_TOOLBAR + 14)
	#define	IDM_EX_LINK_NEW 				(IDM_TOOLBAR + 15)
	#define IDM_EX_LINK_DELETE				(IDM_TOOLBAR + 16)
	#define	IDM_EX_LINK_EDIT				(IDM_TOOLBAR + 17)


/* Dialog IDs */

#define		IDD_EXPLORER_DLG  30500
#define		IDD_FAVES_DLG					(IDD_EXPLORER_DLG + 1)
    #define IDC_TREE_FOLDER					(IDD_EXPLORER_DLG + 2)
    #define IDC_BUTTON_SPLITTER             (IDD_EXPLORER_DLG + 3)
    #define IDC_LIST_FILE   				(IDD_EXPLORER_DLG + 4)
    #define IDC_COMBO_FILTER                (IDD_EXPLORER_DLG + 5)

#define    IDD_NEW_DLG		 30600
	#define IDC_EDIT_NEW                    (IDD_NEW_DLG + 1)
	#define IDC_STATIC_NEW_DESC				(IDD_NEW_DLG + 2)

#define    IDD_PROP_DLG		 30610
	#define IDC_EDIT_NAME                   (IDD_PROP_DLG + 1)
	#define IDC_EDIT_LINK                   (IDD_PROP_DLG + 2)
	#define IDC_STATIC_FAVES_DESC           (IDD_PROP_DLG + 3)
	#define IDC_BTN_OPENDLG                 (IDD_PROP_DLG + 4)
	#define IDC_STATIC_SELECT               (IDD_PROP_DLG + 5)
	#define IDC_TREE_SELECT                 (IDD_PROP_DLG + 6)
	#define IDC_BUTTON_DETAILS              (IDD_PROP_DLG + 7)
	#define IDC_STATIC_NAME                 (IDD_PROP_DLG + 8)
	#define IDC_STATIC_LINK                 (IDD_PROP_DLG + 9)
	#define IDC_BTN_CHOOSEFONT              (IDD_PROP_DLG + 10)

#define		IDD_QUICK_OPEN_DLG   30630
	#define IDC_EDIT_SEARCH					(IDD_QUICK_OPEN_DLG + 1)
	#define IDC_LIST_RESULTS				(IDD_QUICK_OPEN_DLG + 2)

#define		IDD_OPTION_DLG   30650
	#define IDC_CHECK_BRACES                (IDD_OPTION_DLG + 1)
	#define IDC_CHECK_AUTO                  (IDD_OPTION_DLG + 2)
	#define IDC_CHECK_LONG                  (IDD_OPTION_DLG + 3)
	#define IDC_COMBO_SIZE_FORMAT           (IDD_OPTION_DLG + 4)
	#define IDC_COMBO_DATE_FORMAT           (IDD_OPTION_DLG + 5)
	#define IDC_CHECK_SEPEXT                (IDD_OPTION_DLG + 6)
	#define IDC_CHECK_HIDDEN                (IDD_OPTION_DLG + 7)
	#define IDC_STATIC_LONG                 (IDD_OPTION_DLG + 8)
	#define IDC_CHECK_USEICON               (IDD_OPTION_DLG + 9)
	#define IDC_STATIC_SIZE                 (IDD_OPTION_DLG + 11)
	#define IDC_STATIC_DATE                 (IDD_OPTION_DLG + 12)
	#define IDC_STATIC_FILELIST             (IDD_OPTION_DLG + 13)
	#define IDC_STATIC_GENOPT               (IDD_OPTION_DLG + 14)
    #define IDC_CHECK_USEFLUENTICONS        (IDD_OPTION_DLG + 15)
	#define IDC_STATIC_NPPEXEC              (IDD_OPTION_DLG + 16)
	#define IDC_EDIT_EXECNAME				(IDD_OPTION_DLG + 17)
	#define IDC_STATIC_EXECNAME				(IDD_OPTION_DLG + 18)
	#define IDC_EDIT_SCRIPTPATH             (IDD_OPTION_DLG + 19)
	#define IDC_STATIC_SCRIPTPATH           (IDD_OPTION_DLG + 20)
	#define IDC_BTN_EXAMPLE_FILE			(IDD_OPTION_DLG + 21)
	#define IDC_STATIC_COMMANDPROMPT	    (IDD_OPTION_DLG + 22)
	#define IDC_EDIT_CPH        		    (IDD_OPTION_DLG + 23)
	#define IDC_STATIC_CPHNAME     		    (IDD_OPTION_DLG + 24)
	#define IDC_CHECK_AUTONAV     		    (IDD_OPTION_DLG + 25)
	#define IDC_EDIT_HISTORYSIZE			(IDD_OPTION_DLG + 26)
	#define IDC_STATIC_HISTORY				(IDD_OPTION_DLG + 27)
    #define IDC_CHECK_USEFULLTREE           (IDD_OPTION_DLG + 28)
    #define IDC_CHECK_HIDE_FOLDERS          (IDD_OPTION_DLG + 29)
    #define IDC_TAB_OPTION                  (IDD_OPTION_DLG + 30)
    #define IDC_STATIC_WORKSPACE_DIRS       (IDD_OPTION_DLG + 31)
    #define IDC_LIST_WORKSPACE_DIRS         (IDD_OPTION_DLG + 32)
    #define IDC_BTN_ADD_WORKSPACE           (IDD_OPTION_DLG + 33)
    #define IDC_BTN_DEL_WORKSPACE           (IDD_OPTION_DLG + 34)
    #define IDC_BTN_UP_WORKSPACE            (IDD_OPTION_DLG + 35)
    #define IDC_BTN_DOWN_WORKSPACE          (IDD_OPTION_DLG + 36)

/* Fluent Toolbar Icons (17 icons * 4 variants = 68 IDs) */
#define IDI_FL_FAVORITES                1200
#define IDI_FL_FAVORITES_GRAY           1201
#define IDI_FL_FAVORITES_DARK           1202
#define IDI_FL_FAVORITES_DARK_GRAY      1203

#define IDI_FL_PREV                     1204
#define IDI_FL_PREV_GRAY                1205
#define IDI_FL_PREV_DARK                1206
#define IDI_FL_PREV_DARK_GRAY           1207

#define IDI_FL_NEXT                     1208
#define IDI_FL_NEXT_GRAY                1209
#define IDI_FL_NEXT_DARK                1210
#define IDI_FL_NEXT_DARK_GRAY           1211

#define IDI_FL_WORKSPACE                1212
#define IDI_FL_WORKSPACE_GRAY           1213
#define IDI_FL_WORKSPACE_DARK           1214
#define IDI_FL_WORKSPACE_DARK_GRAY      1215

#define IDI_FL_FILENEW                  1216
#define IDI_FL_FILENEW_GRAY             1217
#define IDI_FL_FILENEW_DARK             1218
#define IDI_FL_FILENEW_DARK_GRAY        1219

#define IDI_FL_FOLDERNEW                1220
#define IDI_FL_FOLDERNEW_GRAY           1221
#define IDI_FL_FOLDERNEW_DARK           1222
#define IDI_FL_FOLDERNEW_DARK_GRAY      1223

#define IDI_FL_FIND                     1224
#define IDI_FL_FIND_GRAY                1225
#define IDI_FL_FIND_DARK                1226
#define IDI_FL_FIND_DARK_GRAY           1227

#define IDI_FL_TERMINAL                 1228
#define IDI_FL_TERMINAL_GRAY            1229
#define IDI_FL_TERMINAL_DARK            1230
#define IDI_FL_TERMINAL_DARK_GRAY       1231

#define IDI_FL_FOLDERGO                 1232
#define IDI_FL_FOLDERGO_GRAY            1233
#define IDI_FL_FOLDERGO_DARK            1234
#define IDI_FL_FOLDERGO_DARK_GRAY       1235

#define IDI_FL_FOLDERUSER               1236
#define IDI_FL_FOLDERUSER_GRAY          1237
#define IDI_FL_FOLDERUSER_DARK          1238
#define IDI_FL_FOLDERUSER_DARK_GRAY     1239

#define IDI_FL_UPDATE                   1240
#define IDI_FL_UPDATE_GRAY              1241
#define IDI_FL_UPDATE_DARK              1242
#define IDI_FL_UPDATE_DARK_GRAY         1243

#define IDI_FL_EXPLORER                 1244
#define IDI_FL_EXPLORER_GRAY            1245
#define IDI_FL_EXPLORER_DARK            1246
#define IDI_FL_EXPLORER_DARK_GRAY       1247

#define IDI_FL_LINKNEWFILE              1248
#define IDI_FL_LINKNEWFILE_GRAY         1249
#define IDI_FL_LINKNEWFILE_DARK         1250
#define IDI_FL_LINKNEWFILE_DARK_GRAY    1251

#define IDI_FL_LINKNEWFOLDER            1252
#define IDI_FL_LINKNEWFOLDER_GRAY       1253
#define IDI_FL_LINKNEWFOLDER_DARK       1254
#define IDI_FL_LINKNEWFOLDER_DARK_GRAY  1255

#define IDI_FL_LINKNEW                  1256
#define IDI_FL_LINKNEW_GRAY             1257
#define IDI_FL_LINKNEW_DARK             1258
#define IDI_FL_LINKNEW_DARK_GRAY        1259

#define IDI_FL_LINKDELETE               1260
#define IDI_FL_LINKDELETE_GRAY          1261
#define IDI_FL_LINKDELETE_DARK          1262
#define IDI_FL_LINKDELETE_DARK_GRAY     1263

#define IDI_FL_LINKEDIT                 1264
#define IDI_FL_LINKEDIT_GRAY            1265
#define IDI_FL_LINKEDIT_DARK            1266
#define IDI_FL_LINKEDIT_DARK_GRAY       1267

#define    IDD_HELP_DLG	     30700
    #define IDC_EMAIL_LINK                  (IDD_HELP_DLG + 1)
	#define IDC_NPP_PLUGINS_URL				(IDD_HELP_DLG + 2)
	#define IDC_STATIC_AUTHOR               (IDD_HELP_DLG + 3)
	#define IDC_STATIC_MAIL                 (IDD_HELP_DLG + 4)
	#define IDC_STATIC_THXTO                (IDD_HELP_DLG + 5)
	#define IDC_STATIC_MENU                 (IDD_HELP_DLG + 6)
	#define IDC_STATIC_EXP                  (IDD_HELP_DLG + 7)
	#define IDC_STATIC_FAV                  (IDD_HELP_DLG + 8)
	#define IDC_STATIC_GOTO                 (IDD_HELP_DLG + 9)
	#define IDC_STATIC_CLEAR                (IDD_HELP_DLG + 10)
	#define IDC_STATIC_OPT                  (IDD_HELP_DLG + 11)
	#define IDC_STATIC_HELP                 (IDD_HELP_DLG + 12)
	#define IDC_STATIC_VERSION              (IDD_HELP_DLG + 13)


/* Explorer messages */

#define	   EXX_MESSAGES		 30800
	#define EXM_CHANGECOMBO					(EXX_MESSAGES + 1)
	#define EXM_OPENDIR						(EXX_MESSAGES + 2)
	#define EXM_UPDATE_OVERICON				(EXX_MESSAGES + 6)
	#define EXM_QUERYDROP					(EXX_MESSAGES + 7)
	#define EXM_DRAGLEAVE					(EXX_MESSAGES + 8)
	#define	EXM_OPENLINK					(EXX_MESSAGES + 9)
	#define	EXM_USER_ICONBAR				(EXX_MESSAGES + 10)
    #define EXM_ASYNCTASK_COMPLETED         (EXX_MESSAGES + 11)
    #define EXM_UPDATE_ICON_RESULT          (EXX_MESSAGES + 12)

#define	   EXX_TIMERS		 30820
	#define EXT_UPDATEDEVICE				(EXX_TIMERS + 1)
	#define EXT_SELCHANGE					(EXX_TIMERS + 2)
	#define EXT_UPDATEACTIVATE				(EXX_TIMERS + 3)
	#define	EXT_UPDATEACTIVATEPATH			(EXX_TIMERS + 4)
	#define EXT_SEARCHFILE					(EXX_TIMERS + 5)
	#define EXT_SCROLLLISTUP				(EXX_TIMERS + 6)
	#define EXT_SCROLLLISTDOWN				(EXX_TIMERS + 7)
	#define EXT_AUTOGOTOFILE				(EXX_TIMERS + 8)


#ifndef IDC_STATIC
#define IDC_STATIC							-1
#endif
