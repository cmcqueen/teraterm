/*
 * Copyright (C) 1994-1998 T. Teranishi
 * (C) 2024- TeraTerm Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* serial dialog */

#include <stdio.h>
#include <string.h>
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <assert.h>

#include "tttypes.h"
#include "ttlib.h"
#include "dlglib.h"
#include "ttcommon.h"
#include "dlg_res.h"
#include "tipwin.h"
#include "comportinfo.h"
#include "helpid.h"
#include "asprintf.h"
#include "win32helper.h"
#include "compat_win.h"
#include "ttwinman.h"

#include "serial_pp.h"

// �e���v���[�g�̏����������s��
#define REWRITE_TEMPLATE

static const char *BaudList[] = {
	"110","300","600","1200","2400","4800","9600",
	"14400","19200","38400","57600","115200",
	"230400", "460800", "921600", NULL};
static const char *DataList[] = {"7 bit","8 bit",NULL};
static const char *ParityList[] = {"none", "odd", "even", "mark", "space", NULL};
static const char *StopList[] = {"1 bit", "2 bit", NULL};
static const char *FlowList[] = {"Xon/Xoff", "RTS/CTS", "DSR/DTR", "none", NULL};

typedef struct {
	PTTSet pts;
	ComPortInfo_t *ComPortInfoPtr;
	int ComPortInfoCount;
	HINSTANCE hInst;
	DLGTEMPLATE *dlg_templ;
	int g_deltaSumSerialDlg;					// �}�E�X�z�C�[����Delta�ݐϗp
	WNDPROC g_defSerialDlgEditWndProc;			// Edit Control�̃T�u�N���X���p
	WNDPROC g_defSerialDlgSpeedComboboxWndProc; // Combo-box Control�̃T�u�N���X���p
	TipWin *g_SerialDlgSpeedTip;
	BOOL show_all_port;
} SerialDlgData;

/*
 * �V���A���|�[�g�ݒ�_�C�A���O�̃e�L�X�g�{�b�N�X��COM�|�[�g�̏ڍ׏���\������B
 */
static void serial_dlg_set_comport_info(HWND dlg, SerialDlgData *dlg_data, int port_info_index)
{
#if 0
	for (int i = 0; i < dlg_data->ComPortInfoCount; i++) {
		const ComPortInfo_t *p = &dlg_data->ComPortInfoPtr[i];
		if (p->port_no == port_no) {
			SetDlgItemTextW(dlg, IDC_SERIALTEXT, p->property);
			return;
		}
	}
#endif
	if (port_info_index < dlg_data->ComPortInfoCount) {
		const ComPortInfo_t *p = &dlg_data->ComPortInfoPtr[port_info_index];
		SetDlgItemTextW(dlg, IDC_SERIALTEXT, p->property);
	}
	else {
		SetDlgItemTextW(dlg, IDC_SERIALTEXT, L"This port does not exist now.");
	}
}

/*
 * �V���A���|�[�g�ݒ�_�C�A���O�̃e�L�X�g�{�b�N�X�̃v���V�[�W��
 */
static LRESULT CALLBACK SerialDlgEditWindowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	SerialDlgData *dlg_data = (SerialDlgData *)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

	switch (msg) {
		case WM_KEYDOWN:
			// Edit control��� CTRL+A ����������ƁA�e�L�X�g��S�I������B
			if (wp == 'A' && GetKeyState(VK_CONTROL) < 0) {
				PostMessage(hWnd, EM_SETSEL, 0, -1);
				return 0;
			}
			break;

		case WM_MOUSEWHEEL: {
			// CTRLorSHIFT + �}�E�X�z�C�[���̏ꍇ�A���X�N���[��������B
			WORD keys;
			short delta;
			BOOL page;
			keys = GET_KEYSTATE_WPARAM(wp);
			delta = GET_WHEEL_DELTA_WPARAM(wp);
			page = keys & (MK_CONTROL | MK_SHIFT);

			if (page == 0)
				break;

			dlg_data->g_deltaSumSerialDlg += delta;

			if (dlg_data->g_deltaSumSerialDlg >= WHEEL_DELTA) {
				dlg_data->g_deltaSumSerialDlg -= WHEEL_DELTA;
				SendMessage(hWnd, WM_HSCROLL, SB_PAGELEFT , 0);
			} else if (dlg_data->g_deltaSumSerialDlg <= -WHEEL_DELTA) {
				dlg_data->g_deltaSumSerialDlg += WHEEL_DELTA;
				SendMessage(hWnd, WM_HSCROLL, SB_PAGERIGHT, 0);
			}

			break;
		}
	}
	return CallWindowProc(dlg_data->g_defSerialDlgEditWndProc, hWnd, msg, wp, lp);
}

/*
 * �V���A���|�[�g�ݒ�_�C�A���O��SPEED(BAUD)�̃v���V�[�W��
 */
static LRESULT CALLBACK SerialDlgSpeedComboboxWindowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const int tooltip_timeout = 1000;  // msec
	int h;
	int cx, cy;
	RECT wr;
	SerialDlgData *dlg_data = (SerialDlgData *)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
	int frame_width;

	switch (msg) {
		case WM_MOUSEMOVE:
			// �c�[���`�b�v���쐬
			if (dlg_data->g_SerialDlgSpeedTip == NULL) {
				const wchar_t *UILanguageFileW;
				wchar_t *uimsg;
				UILanguageFileW = dlg_data->pts->UILanguageFileW;
				uimsg = TTGetLangStrW("Tera Term",
									  "DLG_SERIAL_SPEED_TOOLTIP", L"You can directly specify a number", UILanguageFileW);
				dlg_data->g_SerialDlgSpeedTip = TipWinCreate(hInst, hWnd);
				TipWinSetTextW(dlg_data->g_SerialDlgSpeedTip, uimsg);

				free(uimsg);
			}

			// Combo-box�̍�����W�����߂�
			GetWindowRect(hWnd, &wr);

			// �c�[���`�b�v��\������
			TipWinGetWindowSize(dlg_data->g_SerialDlgSpeedTip, NULL, &h);
			TipWinGetFrameSize(dlg_data->g_SerialDlgSpeedTip, &frame_width);
			cx = wr.left;
			cy = wr.top - (h + frame_width * 4);
			TipWinSetPos(dlg_data->g_SerialDlgSpeedTip, cx, cy);
			TipWinSetHideTimer(dlg_data->g_SerialDlgSpeedTip, tooltip_timeout);
			if (!TipWinIsVisible(dlg_data->g_SerialDlgSpeedTip))
				TipWinSetVisible(dlg_data->g_SerialDlgSpeedTip, TRUE);

			break;
	}
	return CallWindowProc(dlg_data->g_defSerialDlgSpeedComboboxWndProc, hWnd, msg, wp, lp);
}

/**
 *	�V���A���|�[�g�h���b�v�_�E����ݒ肷��
 *	ITEMDATA
 *		0...	PortInfo�̐擪����̔ԍ� 0����PortInfoCount-1�܂�
 *		-1		���ݑ��݂��Ȃ��|�[�g
 *		-2		"���݂���|�[�g�̂ݕ\��"
 *		-3		"�S�Ẵ|�[�g��\��"
 */
static void SetPortDrop(HWND hWnd, int id, SerialDlgData *dlg_data)
{
	PTTSet ts = dlg_data->pts;
	BOOL show_all_port = dlg_data->show_all_port;
	int max_com = ts->MaxComPort;
	int sel_index = 0;
	int drop_count = 0;
	if (dlg_data->ComPortInfoCount == 0) {
		// �|�[�g�����݂��Ă��Ȃ��Ƃ��͂��ׂĕ\������
		show_all_port = TRUE;
	}

	// "COM%d" �ł͂Ȃ��|�[�g
	int port_index = 0;
	const ComPortInfo_t *port_info = dlg_data->ComPortInfoPtr;
	const ComPortInfo_t *p;
	for (int i = 0; i < dlg_data->ComPortInfoCount; i++) {
		p = port_info + port_index;
		if (wcsncmp(p->port_name, L"COM", 3) == 0) {
			break;
		}
		port_index++;
		wchar_t *name;
		if (p->friendly_name != NULL) {
			aswprintf(&name, L"%s: %s", p->port_name, p->friendly_name);
		}
		else {
			aswprintf(&name, L"%s: (no name, exists)", p->port_name);
		}
		int index = SendDlgItemMessageW(hWnd, id, CB_ADDSTRING, 0, (LPARAM)name);
		free(name);
		SendDlgItemMessageA(hWnd, id, CB_SETITEMDATA, index, i);
		drop_count++;
	}

	// �`���I�� "COM%d"
	BOOL all = show_all_port;
	for (int j = 1;; j++) {
		if (j >= max_com) {
			all = FALSE;
		}

		wchar_t *com_name;
		int item_data;
		if (port_index == dlg_data->ComPortInfoCount) {
			if (!all) {
				break;
			}
			aswprintf(&com_name, L"COM%d", j);
			item_data = -1;
		}
		else {
			p = port_info + port_index;

			if (p->port_no == j) {
				item_data = port_index;
				port_index++;

				if (p->friendly_name != NULL) {
					aswprintf(&com_name, L"%s: %s", p->port_name, p->friendly_name);
				}
				else {
					aswprintf(&com_name, L"%s: (no friendly name, exist)", p->port_name);
				}
			}
			else {
				if (j != ts->ComPort && !all) {
					continue;
				}
				aswprintf(&com_name, L"COM%d", j);
				item_data = -1;
			}
		}

		if (j == ts->ComPort) {
			sel_index = drop_count;
		}
		int index = SendDlgItemMessageW(hWnd, id, CB_ADDSTRING, 0, (LPARAM)com_name);
		SendDlgItemMessageA(hWnd, id, CB_SETITEMDATA, index, item_data);
		drop_count++;
	}
#if 0
	if (show_all_port) {
		int index = SendDlgItemMessageW(hWnd, id, CB_ADDSTRING, 0, (LPARAM)L"<���݂���|�[�g�̂ݕ\��>");
		SendDlgItemMessageA(hWnd, id, CB_SETITEMDATA, index, -2);
	}
	else {
		wchar_t *s;
		aswprintf(&s, L"<COM%d�܂ŕ\��>", max_com);
		int index = SendDlgItemMessageW(hWnd, id, CB_ADDSTRING, 0, (LPARAM)s);
		free(s);
		SendDlgItemMessageA(hWnd, id, CB_SETITEMDATA, index, -3);
	}
#endif

	serial_dlg_set_comport_info(hWnd, dlg_data, ts->ComPort);

	ExpandCBWidth(hWnd, id);
	if (dlg_data->ComPortInfoCount == 0) {
		// COM�|�[�g�Ȃ�
		//EnableWindow(GetDlgItem(hWnd, id), FALSE);
		serial_dlg_set_comport_info(hWnd, dlg_data, -1);
	}
	else {
		if (cv.Open && (cv.PortType == IdSerial)) {
			// �ڑ����̎��͑I���ł��Ȃ��悤�ɂ���
			EnableWindow(GetDlgItem(hWnd, id), FALSE);
		}
		SendDlgItemMessage(hWnd, id, CB_SETCURSEL, sel_index, 0);
	}
}

/*
 * �V���A���|�[�g�ݒ�_�C�A���O
 *
 * �V���A���|�[�g����0�̎��͌Ă΂�Ȃ�
 */
static INT_PTR CALLBACK SerialDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ IDC_SERIALPORT_LABEL, "DLG_SERIAL_PORT" },
		{ IDC_SERIALBAUD_LEBAL, "DLG_SERIAL_BAUD" },
		{ IDC_SERIALDATA_LABEL, "DLG_SERIAL_DATA" },
		{ IDC_SERIALPARITY_LABEL, "DLG_SERIAL_PARITY" },
		{ IDC_SERIALSTOP_LABEL, "DLG_SERIAL_STOP" },
		{ IDC_SERIALFLOW_LABEL, "DLG_SERIAL_FLOW" },
		{ IDC_SERIALDELAY, "DLG_SERIAL_DELAY" },
		{ IDC_SERIALDELAYCHAR_LABEL, "DLG_SERIAL_DELAYCHAR" },
		{ IDC_SERIALDELAYLINE_LABEL, "DLG_SERIAL_DELAYLINE" },
	};

	switch (Message) {
		case WM_INITDIALOG: {
			int i, sel;

			SerialDlgData *dlg_data = (SerialDlgData *)(((PROPSHEETPAGEW_V1 *)lParam)->lParam);
			SetWindowLongPtrW(Dialog, DWLP_USER, (LPARAM)dlg_data);
			PTTSet ts = dlg_data->pts;

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts->UILanguageFileW);

			SetPortDrop(Dialog, IDC_SERIALPORT, dlg_data);
#if 0
			if (dlg_data->ComPortInfoCount > 0) {
				// COM�|�[�g����
				int w = 0;
				for (i = 0; i < dlg_data->ComPortInfoCount; i++) {
					ComPortInfo_t *p = dlg_data->ComPortInfoPtr + i;

					// MaxComPort ���z����|�[�g�͕\�����Ȃ�
					if (i > ts->MaxComPort) {
						continue;
					}

					SendDlgItemMessageW(Dialog, IDC_SERIALPORT, CB_ADDSTRING, 0, (LPARAM)p->port_name);

					if (p->port_no == ts->ComPort) {
						w = i;
					}
				}
				SendDlgItemMessage(Dialog, IDC_SERIALPORT, CB_SETCURSEL, w, 0);
				serial_dlg_set_comport_info(Dialog, dlg_data, w);

				if (cv.Open && (cv.PortType == IdSerial)) {
					// �ڑ����̎��͑I���ł��Ȃ��悤�ɂ���
					EnableWindow(GetDlgItem(Dialog, IDC_SERIALPORT), FALSE);
				}
			}
			else {
				// COM�|�[�g�Ȃ�
				EnableWindow(GetDlgItem(Dialog, IDC_SERIALPORT), FALSE);
				serial_dlg_set_comport_info(Dialog, dlg_data, 0);
			}
#endif
			SetDropDownList(Dialog, IDC_SERIALBAUD, BaudList, 0);
			i = sel = 0;
			while (BaudList[i] != NULL) {
				if ((WORD)atoi(BaudList[i]) == ts->Baud) {
					SendDlgItemMessage(Dialog, IDC_SERIALBAUD, CB_SETCURSEL, i, 0);
					sel = 1;
					break;
				}
				i++;
			}
			if (!sel) {
				SetDlgItemInt(Dialog, IDC_SERIALBAUD, ts->Baud, FALSE);
			}

			SetDropDownList(Dialog, IDC_SERIALDATA, DataList, ts->DataBit);
			SetDropDownList(Dialog, IDC_SERIALPARITY, ParityList, ts->Parity);
			SetDropDownList(Dialog, IDC_SERIALSTOP, StopList, ts->StopBit);

			/*
			 * value               display
			 * 1 IdFlowX           1 Xon/Xoff
			 * 2 IdFlowHard        2 RTS/CTS
			 * 3 IdFlowNone        4 none
			 * 4 IdFlowHardDsrDtr  3 DSR/DTR
			 */
			WORD Flow = ts->Flow;
			if (Flow == 3) {
				Flow = 4;
			}
			else if (Flow == 4) {
				Flow = 3;
			}
			SetDropDownList(Dialog, IDC_SERIALFLOW, FlowList, Flow);

			SetDlgItemInt(Dialog,IDC_SERIALDELAYCHAR,ts->DelayPerChar,FALSE);
			SendDlgItemMessage(Dialog, IDC_SERIALDELAYCHAR, EM_LIMITTEXT,4, 0);

			SetDlgItemInt(Dialog,IDC_SERIALDELAYLINE,ts->DelayPerLine,FALSE);
			SendDlgItemMessage(Dialog, IDC_SERIALDELAYLINE, EM_LIMITTEXT,4, 0);

			CenterWindow(Dialog, GetParent(Dialog));

			// Edit control���T�u�N���X������B
			dlg_data->g_deltaSumSerialDlg = 0;
			SetWindowLongPtrW(GetDlgItem(Dialog, IDC_SERIALTEXT), GWLP_USERDATA, (LONG_PTR)dlg_data);
			dlg_data->g_defSerialDlgEditWndProc = (WNDPROC)SetWindowLongPtrW(
				GetDlgItem(Dialog, IDC_SERIALTEXT),
				GWLP_WNDPROC,
				(LONG_PTR)SerialDlgEditWindowProc);

			// Combo-box control���T�u�N���X������B
			SetWindowLongPtrW(GetDlgItem(Dialog, IDC_SERIALBAUD), GWLP_USERDATA, (LONG_PTR)dlg_data);
			dlg_data->g_defSerialDlgSpeedComboboxWndProc = (WNDPROC)SetWindowLongPtrW(
				GetDlgItem(Dialog, IDC_SERIALBAUD),
				GWLP_WNDPROC,
				(LONG_PTR)SerialDlgSpeedComboboxWindowProc);

#if 0
			{
				wchar_t *s = NULL;
				if (cv.Open && (cv.PortType == IdSerial)) {
					aswprintf(&s,
							  L"�V���A���ɐڑ���(COM%d)\r\n"
							  L"�e�p�����[�^�̓V���A���|�[�g�ɐݒ肳��܂�\r\n"
							  L"\r\n",
							  cv.ComPort);
				}
				else {
					if (dlg_data->ComPortInfoCount == 0) {
						wchar_t *t;
						aswprintf(&t,
								  L"�V���A���|�[�g�����݂��܂���\r\n");
						awcscat(&s, t);
					}
					wchar_t *t;
					aswprintf(&t,
							  L"�V�����ڑ��_�C�A���O��\�������Ƃ�\r\n"
							  L"(�|�[�g�����݂��Ă�����)\r\n"
							  L"�f�t�H���g�V���A���|�[�g�ƂȂ�܂��B\r\n"
							  L"�e�p�����[�^�̓V���A���|�[�g�ڑ���\r\n"
							  L"�V���A���|�[�g�ɐݒ肳��܂�\r\n");
					awcscat(&s, t);
				}
				SetDlgItemTextW(Dialog, IDC_SERIAL_CONNECTION_INFO, s);
				free(s);
			}
#else
			ShowWindow(GetDlgItem(Dialog, IDC_SERIAL_CONNECTION_INFO), SW_HIDE);
			EnableWindow(GetDlgItem(Dialog, IDC_SERIAL_CONNECTION_INFO), FALSE);
#endif

			return TRUE;
		}
		case WM_NOTIFY: {
			NMHDR *nmhdr = (NMHDR *)lParam;
			SerialDlgData *data = (SerialDlgData *)GetWindowLongPtrW(Dialog, DWLP_USER);
			PTTSet ts = data->pts;
			switch (nmhdr->code) {
			case PSN_APPLY: {
				int w;
				char Temp[128];
				Temp[0] = 0;
				GetDlgItemTextA(Dialog, IDC_SERIALPORT, Temp, sizeof(Temp)-1);
				if (strncmp(Temp, "COM", 3) == 0 && Temp[3] != '\0') {
					ts->ComPort = (WORD)atoi(&Temp[3]);
				}

				GetDlgItemTextA(Dialog, IDC_SERIALBAUD, Temp, sizeof(Temp)-1);
				if (atoi(Temp) != 0) {
					ts->Baud = (DWORD)atoi(Temp);
				}
				if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALDATA)) > 0) {
					ts->DataBit = w;
				}
				if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALPARITY)) > 0) {
					ts->Parity = w;
				}
				if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALSTOP)) > 0) {
					ts->StopBit = w;
				}
				if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALFLOW)) > 0) {
					/*
					 * display    value
					 * 1 Xon/Xoff 1 IdFlowX
					 * 2 RTS/CTS  2 IdFlowHard
					 * 3 DSR/DTR  4 IdFlowHardDsrDtr
					 * 4 none     3 IdFlowNone
					 */
					WORD Flow = w;
					if (Flow == 3) {
						Flow = 4;
					}
					else if (Flow == 4) {
						Flow = 3;
					}
					ts->Flow = Flow;
				}

				ts->DelayPerChar = GetDlgItemInt(Dialog,IDC_SERIALDELAYCHAR,NULL,FALSE);

				ts->DelayPerLine = GetDlgItemInt(Dialog,IDC_SERIALDELAYLINE,NULL,FALSE);

				// TODO �폜
				// �S�ʃ^�u�́u�W���̃|�[�g�v�Ńf�t�H���g�̐ڑ��|�[�g���w�肷��
				// �����ł͎w�肵�Ȃ�
				// ts->PortType = IdSerial;

				// �{�[���[�g���ύX����邱�Ƃ�����̂ŁA�^�C�g���ĕ\����
				// ���b�Z�[�W���΂��悤�ɂ����B (2007.7.21 maya)
				PostMessage(GetParent(Dialog),WM_USER_CHANGETITLE,0,0);

				break;
			}
			case PSN_HELP: {
				HWND vtwin = GetParent(GetParent(Dialog));
				PostMessage(vtwin, WM_USER_DLGHELP2, HlpMenuSetupAdditionalSerialPort, 0);
				break;
			}
			}
			break;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_SERIALPORT:
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						// ���X�g����COM�|�[�g���I�����ꂽ
						SerialDlgData *dlg_data = (SerialDlgData *)GetWindowLongPtrW(Dialog, DWLP_USER);
						int sel = (int)SendDlgItemMessageA(Dialog, IDC_SERIALPORT, CB_GETCURSEL, 0, 0);
						int item_data = (int)SendDlgItemMessageA(Dialog, IDC_SERIALPORT, CB_GETITEMDATA, sel, 0);
						if (item_data >= 0) {
							// �ڍ׏���\������
							serial_dlg_set_comport_info(Dialog, dlg_data, item_data);
						}
						else if (item_data == -1) {
							// ���Ȃ���\������
							serial_dlg_set_comport_info(Dialog, dlg_data, 10000);
						}
						else {
							// �I����@�ύX
							dlg_data->show_all_port = (item_data == -2) ? FALSE : TRUE;
							SendDlgItemMessageA(Dialog, IDC_SERIALPORT, CB_RESETCONTENT, 0, 0);
							SetPortDrop(Dialog, IDC_SERIALPORT, dlg_data);
						}
						break;
					}

					return TRUE;
			}
	}
	return FALSE;
}

static UINT CALLBACK CallBack(HWND hwnd, UINT uMsg, struct _PROPSHEETPAGEW *ppsp)
{
	UINT ret_val = 0;
	switch (uMsg) {
	case PSPCB_CREATE:
		ret_val = 1;
		break;
	case PSPCB_RELEASE: {
		SerialDlgData *dlg_data = (SerialDlgData *)ppsp->lParam;

		// �c�[���`�b�v��p������
		if (dlg_data->g_SerialDlgSpeedTip) {
			TipWinDestroy(dlg_data->g_SerialDlgSpeedTip);
			dlg_data->g_SerialDlgSpeedTip = NULL;
		}
		free((void *)ppsp->pResource);
		ppsp->pResource = NULL;
		ComPortInfoFree(dlg_data->ComPortInfoPtr, dlg_data->ComPortInfoCount);
		free(dlg_data);
		ppsp->lParam = (LPARAM)NULL;
		break;
	}
	default:
		break;
	}
	return ret_val;
}

HPROPSHEETPAGE SerialPageCreate(HINSTANCE inst, TTTSet *pts)
{
	SerialDlgData *Param = (SerialDlgData *)calloc(1, sizeof(SerialDlgData));
	Param->hInst = inst;
	Param->pts = pts;
	Param->ComPortInfoPtr = ComPortInfoGet(&Param->ComPortInfoCount);
	//Param->show_all_port = FALSE;
	Param->show_all_port = TRUE;

	PROPSHEETPAGEW_V1 psp = {};
	psp.dwSize = sizeof(psp);
	psp.dwFlags = PSP_DEFAULT | PSP_USECALLBACK | PSP_USETITLE | PSP_HASHELP;
	psp.hInstance = inst;
	psp.pfnCallback = CallBack;
	wchar_t *UIMsg;
	GetI18nStrWW("Tera Term", "DLG_SERIAL_TITLE",
				 L"Serial port", pts->UILanguageFileW, &UIMsg);
	psp.pszTitle = UIMsg;
	psp.pszTemplate = MAKEINTRESOURCEW(IDD_SERIALDLG);
#if defined(REWRITE_TEMPLATE)
	psp.dwFlags |= PSP_DLGINDIRECT;
	Param->dlg_templ = TTGetDlgTemplate(inst, MAKEINTRESOURCEW(IDD_SERIALDLG));
	psp.pResource = Param->dlg_templ;
#endif

	psp.pfnDlgProc = SerialDlg;
	psp.lParam = (LPARAM)Param;

	HPROPSHEETPAGE hpsp = CreatePropertySheetPageW((LPPROPSHEETPAGEW)&psp);
	free(UIMsg);
	return hpsp;
}
