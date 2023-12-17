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
	#define	IDM_EX_LINK_NEW_FILE			(IDM_TOOLBAR + 12)
	#define	IDM_EX_LINK_NEW_FOLDER			(IDM_TOOLBAR + 13)
	#define	IDM_EX_LINK_NEW 				(IDM_TOOLBAR + 14)
	#define IDM_EX_LINK_DELETE				(IDM_TOOLBAR + 15)
	#define	IDM_EX_LINK_EDIT				(IDM_TOOLBAR + 16)



/* Dialog IDs */

#define		IDD_EXPLORER_DLG  30500
#define		IDD_FAVES_DLG					(IDD_EXPLORER_DLG + 1)
	#define IDC_TREE_FOLDER					(IDD_EXPLORER_DLG + 2)
	#define IDC_BUTTON_SPLITTER             (IDD_EXPLORER_DLG + 3)
	#define IDC_LIST_FILE   				(IDD_EXPLORER_DLG + 4)
	#define IDC_STATIC_FILTER				(IDD_EXPLORER_DLG + 5)
    #define IDC_COMBO_FILTER                (IDD_EXPLORER_DLG + 6)

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
	#define IDC_EDIT_TIMEOUT                (IDD_OPTION_DLG + 10)
	#define IDC_STATIC_SIZE                 (IDD_OPTION_DLG + 11)
	#define IDC_STATIC_DATE                 (IDD_OPTION_DLG + 12)
	#define IDC_STATIC_FILELIST             (IDD_OPTION_DLG + 13)
	#define IDC_STATIC_GENOPT               (IDD_OPTION_DLG + 14)
	#define IDC_STATIC_TMO                  (IDD_OPTION_DLG + 15)
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
	#define EXM_OPENFILE					(EXX_MESSAGES + 3)
	#define EXM_UPDATE_PATH					(EXX_MESSAGES + 5)
	#define EXM_UPDATE_OVERICON				(EXX_MESSAGES + 6)
	#define EXM_QUERYDROP					(EXX_MESSAGES + 7)
	#define EXM_DRAGLEAVE					(EXX_MESSAGES + 8)
	#define	EXM_OPENLINK					(EXX_MESSAGES + 9)
	#define	EXM_USER_ICONBAR				(EXX_MESSAGES + 10)

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
