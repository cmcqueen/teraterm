
#include <windows.h>
#include <stdio.h>
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <wchar.h>

#include "i18n.h"
#include "tt_res.h"
#include "ttlib.h"
#include "dlglib.h"
#include "layer_for_unicode.h"
#include "tttypes.h"		// for WM_USER_DLGHELP2
#include "helpid.h"
#include "codeconv.h"
#include "ttftypes.h"		// for TitSendFile

#include "sendfiledlg.h"

/**
 *	GetOpenFileName(), GetSaveFileName() �p�t�B���^������擾
 *
 *	@param[in]	user_filter_mask	���[�U�[�t�B���^������
 *									"*.txt", "*.txt;*.log" �Ȃ�
 *									NULL�̂Ƃ��g�p���Ȃ�
 *	@param[in]	UILanguageFile
 *	@param[out]	len					��������������(wchar_t�P��)
 *									NULL�̂Ƃ��͕Ԃ��Ȃ�
 *	@retval		"User define(*.txt)\0*.txt\0All(*.*)\0*.*\0" �Ȃ�
 *				�I�[�� "\0\0" �ƂȂ�
 */
static wchar_t *GetCommonDialogFilterW(const char *user_filter_mask, const char *UILanguageFile, size_t *len)
{
	// "���[�U��`(*.txt)\0*.txt"
	wchar_t *user_filter_str = NULL;
	size_t user_filter_len = 0;
	if (user_filter_mask != NULL && user_filter_mask[0] != 0) {
		wchar_t user_filter_name[MAX_UIMSG];
		get_lang_msgW("FILEDLG_USER_FILTER_NAME", user_filter_name, sizeof(user_filter_name), L"User define",
					 UILanguageFile);
		size_t user_filter_name_len = wcslen(user_filter_name);
		wchar_t *user_filter_maskW = ToWcharA(user_filter_mask);
		size_t user_filter_mask_len = wcslen(user_filter_maskW);
		user_filter_len = user_filter_name_len + 1 + user_filter_mask_len + 1 + 1 + user_filter_mask_len + 1;
		user_filter_str = (wchar_t *)malloc(user_filter_len * sizeof(wchar_t));
		wchar_t *p = user_filter_str;
		wmemcpy(p, user_filter_name, user_filter_name_len);
		p += user_filter_name_len;
		*p++ = '(';
		wmemcpy(p, user_filter_maskW, user_filter_mask_len);
		p += user_filter_mask_len;
		*p++ = ')';
		*p++ = '\0';
		wmemcpy(p, user_filter_maskW, user_filter_mask_len);
		p += user_filter_mask_len;
		*p++ = '\0';
		free(user_filter_maskW);
	}

	// "���ׂẴt�@�C��(*.*)\0*.*"
	wchar_t all_filter_str[MAX_UIMSG];
	get_lang_msgW("FILEDLG_ALL_FILTER", all_filter_str, _countof(all_filter_str), L"All(*.*)\\0*.*", UILanguageFile);
	size_t all_filter_len;
	{
		size_t all_filter_title_len = wcsnlen(all_filter_str, _countof(all_filter_str));
		if (all_filter_title_len == 0 || all_filter_title_len == _countof(all_filter_str)) {
			all_filter_str[0] = 0;
			all_filter_len = 0;
		} else {
			size_t all_filter_mask_max = _countof(all_filter_str) - all_filter_title_len - 1;
			size_t all_filter_mask_len = wcsnlen(all_filter_str + all_filter_title_len + 1, all_filter_mask_max);
			if (all_filter_mask_len == 0 || all_filter_mask_len == _countof(all_filter_str)) {
				all_filter_str[0] = 0;
				all_filter_len = 0;
			} else {
				all_filter_len = all_filter_title_len + 1 + all_filter_mask_len + 1;
			}
		}
	}

	// �t�B���^����������
	size_t filter_len = user_filter_len + all_filter_len;
	wchar_t* filter_str;
	if (filter_len != 0) {
		filter_len++;
		filter_str = (wchar_t*)malloc(filter_len * sizeof(wchar_t));
		wchar_t *p = filter_str;
		if (user_filter_len != 0) {
			wmemcpy(p, user_filter_str, user_filter_len);
			p += user_filter_len;
		}
		wmemcpy(p, all_filter_str, all_filter_len);
		p += all_filter_len;
		*p = '\0';
	} else {
		filter_len = 2;
		filter_str = (wchar_t*)malloc(filter_len * sizeof(wchar_t));
		filter_str[0] = 0;
		filter_str[1] = 0;
	}

	if (user_filter_len != 0) {
		free(user_filter_str);
	}

	if (len != NULL) {
		*len = filter_len;
	}
	return filter_str;
}

static INT_PTR CALLBACK SendFileDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static const DlgTextInfo TextInfos[] = {
		{0, "FILEDLG_TRANS_TITLE_SENDFILE"},
		{IDC_SENDFILE_FILENAME_TITLE, "DLG_SENDFILE_FILENAME_TITLE"},
		{IDC_SENDFILE_CHECK_BINARY, "DLG_FOPT_BINARY"},
		{IDC_SENDFILE_DELAYTYPE_LABEL, "test"},
		{IDC_SENDFILE_SEND_SIZE_LABEL, "test"},
		{IDC_SENDFILE_DELAYTIME_LABEL, "test"},
		{IDCANCEL, "BTN_CANCEL"},
		{IDOK, "BTN_OK"},
	};
	static const I18nTextInfo l[] = {
		{"DLG_SENDFILE_DELAYTYPE_NO_DELAY", L"no delay"},
		{"DLG_SENDFILE_DELAYTYPE_PER_CHAR", L"per charactor"},
		{"DLG_SENDFILE_DELAYTYPE_PER_LINE", L"per line"},
		{"DLG_SENDFILE_DELAYTYPE_PER_SENDSIZE", L"per sendsize"},
	};
	static const int send_size_list[] = {16, 256, 4 * 1024};
	sendfiledlgdata *data = (sendfiledlgdata *)GetWindowLongPtr(hDlgWnd, DWLP_USER);
	int i;

	switch (msg) {
		case WM_INITDIALOG:
			data = (sendfiledlgdata *)lp;
			data->MsgDlgHelp = RegisterWindowMessage(HELPMSGSTRING);
			SetWindowLongPtr(hDlgWnd, DWLP_USER, (LONG_PTR)data);
			::DragAcceptFiles(hDlgWnd, TRUE);
			SetDlgTexts(hDlgWnd, TextInfos, _countof(TextInfos), data->UILanguageFile);
			CenterWindow(hDlgWnd, GetParent(hDlgWnd));

			SetI18nList("TeraTerm", hDlgWnd, IDC_SENDFILE_DELAYTYPE_DROPDOWN, l, _countof(l), data->UILanguageFile, 0);

			for (i = 0; i < _countof(send_size_list); i++) {
				char buf[32];
				sprintf(buf, "%d", send_size_list[i]);
				SendDlgItemMessageA(hDlgWnd, IDC_SENDFILE_SEND_SIZE_DROPDOWN, CB_ADDSTRING, 0, (LPARAM)buf);
			}
			SendDlgItemMessage(hDlgWnd, IDC_SENDFILE_SEND_SIZE_DROPDOWN, CB_SETCURSEL, _countof(send_size_list) - 1, 0);

			SetDlgItemTextA(hDlgWnd, IDC_SENDFILE_DELAYTIME_EDIT, "1");

			EnableWindow(GetDlgItem(hDlgWnd, IDC_SENDFILE_SEND_SIZE_DROPDOWN), FALSE);
			EnableWindow(GetDlgItem(hDlgWnd, IDC_SENDFILE_DELAYTIME_EDIT), FALSE);

			return TRUE;

		case WM_COMMAND:
			switch (wp) {
				case IDOK | (BN_CLICKED << 16): {
					size_t len = _SendDlgItemMessageW(hDlgWnd, IDC_SENDFILE_FILENAME_EDIT, WM_GETTEXTLENGTH, 0, 0);
					len++;  // for '\0'
					wchar_t *strW = (wchar_t *)malloc(sizeof(wchar_t) * len);
					if (strW != NULL) {
						_GetDlgItemTextW(hDlgWnd, IDC_SENDFILE_FILENAME_EDIT, strW, (int)len);
						strW[len - 1] = '\0';
					}

					const DWORD attr = _GetFileAttributesW(strW);
					if (attr == INVALID_FILE_ATTRIBUTES || attr & FILE_ATTRIBUTE_DIRECTORY) {
						wchar_t caption[MAX_UIMSG];
						wchar_t message[MAX_UIMSG];

						get_lang_msgW("MSG_TT_ERROR", caption, _countof(caption), L"Tera Term: Error",
									  data->UILanguageFile);
						get_lang_msgW("MSG_CANTOPEN_FILE_ERROR", message, _countof(message), L"Cannot open file",
									  data->UILanguageFile);
						_MessageBoxW(hDlgWnd, message, caption, MB_TASKMODAL | MB_ICONEXCLAMATION);

						free(strW);

						PostMessage(hDlgWnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlgWnd, IDC_SENDFILE_FILENAME_EDIT),
									TRUE);

						return TRUE;
					}

					data->filename = strW;
					data->binary =
						SendMessage(GetDlgItem(hDlgWnd, IDC_SENDFILE_CHECK_BINARY), BM_GETCHECK, 0, 0) == BST_CHECKED
							? TRUE
							: FALSE;
					data->delay_type = (int)SendDlgItemMessage(hDlgWnd, IDC_SENDFILE_DELAYTYPE_DROPDOWN, CB_GETCURSEL, 0, 0);
					data->delay_tick = GetDlgItemInt(hDlgWnd, IDC_SENDFILE_DELAYTIME_EDIT, NULL, FALSE);
					data->send_size = GetDlgItemInt(hDlgWnd, IDC_SENDFILE_SEND_SIZE_DROPDOWN, NULL, FALSE);

					TTEndDialog(hDlgWnd, IDOK);
					return TRUE;
				}

				case IDHELP | (BN_CLICKED << 16):
					PostMessage(GetParent(hDlgWnd), WM_USER_DLGHELP2, HlpMenuFileSendfile, 0);
					return TRUE;

				case IDCANCEL | (BN_CLICKED << 16):
					data->filename = NULL;
					TTEndDialog(hDlgWnd, IDCANCEL);
					return TRUE;

				case IDC_SENDFILE_FILENAME_BUTTON | (BN_CLICKED << 16): {
					char TempDir[MAX_PATH];
					GetCurrentDirectoryA(sizeof(TempDir), TempDir);

					char title[40];
					char uimsg[MAX_UIMSG];
					get_lang_msg("FILEDLG_TRANS_TITLE_SENDFILE", uimsg, sizeof(uimsg), TitSendFile,
								 data->UILanguageFile);
					strcpy_s(title, "Tera Term: ");
					strncat_s(title, _countof(title), uimsg, _TRUNCATE);

					size_t filter_len;
					wchar_t *filterW = GetCommonDialogFilterW(data->filesend_filter, data->UILanguageFile, &filter_len);
					char *filterA = _WideCharToMultiByte(filterW, filter_len, CP_ACP, NULL);
					free(filterW);

					char filename[MAX_PATH];
					filename[0] = 0;
					OPENFILENAME ofn = {};
					ofn.lStructSize = get_OPENFILENAME_SIZE();
					ofn.hwndOwner = hDlgWnd;
					ofn.lpstrFile = filename;
					ofn.nMaxFile = sizeof(filename);
					ofn.nMaxFile = MAX_PATH;
					ofn.lpstrFilter = filterA;
					ofn.nFilterIndex = 0;
					ofn.lpstrTitle = title;
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_SHOWHELP | OFN_HIDEREADONLY;
					BOOL Ok = GetOpenFileNameA(&ofn);
					free(filterA);

					SetCurrentDirectoryA(TempDir);

					if (Ok) {
						SetDlgItemTextA(hDlgWnd, IDC_SENDFILE_FILENAME_EDIT, filename);
						PostMessage(hDlgWnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlgWnd, IDOK), TRUE);
					}

					return TRUE;
				}

				case IDC_SENDFILE_DELAYTYPE_DROPDOWN | (CBN_SELCHANGE << 16): {
					int sel = (int)SendDlgItemMessage(hDlgWnd, IDC_SENDFILE_DELAYTYPE_DROPDOWN, CB_GETCURSEL, 0, 0);
					EnableWindow(GetDlgItem(hDlgWnd, IDC_SENDFILE_SEND_SIZE_DROPDOWN), sel != 3 ? FALSE : TRUE);
					EnableWindow(GetDlgItem(hDlgWnd, IDC_SENDFILE_DELAYTIME_EDIT), sel == 0 ? FALSE : TRUE);
					return TRUE;
				}

				default:
					return FALSE;
			}
			return FALSE;

		case WM_DROPFILES: {
			// �����h���b�v����Ă��ŏ���1����������
			HDROP hDrop = (HDROP)wp;
			const UINT len = _DragQueryFileW(hDrop, 0, NULL, 0);
			if (len == 0) {
				DragFinish(hDrop);
				return TRUE;
			}
			wchar_t *filename = (wchar_t *)malloc(sizeof(wchar_t) * (len + 1));
			_DragQueryFileW(hDrop, 0, filename, len + 1);
			filename[len] = '\0';
			_SetDlgItemTextW(hDlgWnd, IDC_SENDFILE_FILENAME_EDIT, filename);
			SendDlgItemMessage(hDlgWnd, IDC_SENDFILE_FILENAME_EDIT, EM_SETSEL, len, len);
			PostMessage(hDlgWnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlgWnd, IDOK), TRUE);

			free(filename);
			DragFinish(hDrop);
			return TRUE;
		}
		default:
			if (data != NULL && msg == data->MsgDlgHelp) {
				// �R�����_�C�A���O�Ńw���v�{�^���������ꂽ
				PostMessage(GetParent(hDlgWnd), WM_USER_DLGHELP2, HlpMenuFileSendfile, 0);
				return TRUE;
			}
			return FALSE;
	}
}

INT_PTR sendfiledlg(HINSTANCE hInstance, HWND hWndParent, sendfiledlgdata *data)
{
	INT_PTR ret;
	ret = TTDialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_SENDFILEDLG), hWndParent, SendFileDlgProc, (LPARAM)data);
	return ret;
}