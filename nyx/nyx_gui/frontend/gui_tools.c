/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2026 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <bdk.h>

#include "gui.h"
#include "gui_tools.h"
#include "gui_tools_partition_manager.h"
#include "gui_emmc_tools.h"
#include "fe_emummc_tools.h"
#include "../config.h"
#include "../hos/pkg1.h"
#include "../hos/pkg2.h"
#include "../hos/hos.h"
#include <libs/compr/blz.h>
#include <libs/fatfs/ff.h>

lv_obj_t *ums_mbox;

extern char *emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage);

bool get_set_autorcm_status(bool toggle)
{
	u32 sector;
	u8 corr_mod0, mod1;
	bool enabled = false;

	if (h_cfg.t210b01)
		return false;

	emmc_initialize(false);

	u8 *tempbuf = (u8 *)malloc(0x200);
	emmc_set_partition(EMMC_BOOT0);
	sdmmc_storage_read(&emmc_storage, 0x200 / EMMC_BLOCKSIZE, 1, tempbuf);

	// Get the correct RSA modulus byte masks.
	nx_emmc_get_autorcm_masks(&corr_mod0, &mod1);

	// Check if 2nd byte of modulus is correct.
	if (tempbuf[0x11] != mod1)
		goto out;

	if (tempbuf[0x10] != corr_mod0)
		enabled = true;

	// Toggle autorcm status if requested.
	if (toggle)
	{
		// Iterate BCTs.
		for (u32 i = 0; i < 4; i++)
		{
			sector = (0x200 + (0x4000 * i)) / EMMC_BLOCKSIZE; // 0x4000 bct + 0x200 offset.
			sdmmc_storage_read(&emmc_storage, sector, 1, tempbuf);

			if (!enabled)
				tempbuf[0x10] = 0;
			else
				tempbuf[0x10] = corr_mod0;
			sdmmc_storage_write(&emmc_storage, sector, 1, tempbuf);
		}
		enabled = !enabled;
	}

	// Check if RCM is patched and protect from a possible brick.
	if (enabled && h_cfg.rcm_patched && hw_get_chip_id() != GP_HIDREV_MAJOR_T210B01)
	{
		// Iterate BCTs.
		for (u32 i = 0; i < 4; i++)
		{
			sector = (0x200 + (0x4000 * i)) / EMMC_BLOCKSIZE; // 0x4000 bct + 0x200 offset.
			sdmmc_storage_read(&emmc_storage, sector, 1, tempbuf);

			// Check if 2nd byte of modulus is correct.
			if (tempbuf[0x11] != mod1)
				continue;

			// If AutoRCM is enabled, disable it.
			if (tempbuf[0x10] != corr_mod0)
			{
				tempbuf[0x10] = corr_mod0;

				sdmmc_storage_write(&emmc_storage, sector, 1, tempbuf);
			}
		}

		enabled = false;
	}

out:
	free(tempbuf);
	emmc_end();

	h_cfg.autorcm_enabled = enabled;

	return enabled;
}

lv_res_t _create_mbox_autorcm_status(lv_obj_t *btn)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char * mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);

	bool enabled = get_set_autorcm_status(true);

	char mbox_txt[512];
	if (enabled)
	{
		s_printf(mbox_txt,
			"#008EED 상태 메시지#\n\n"
			"#FFBA00 안내#: 자동 RCM이 #C7EA46 활성화# 되었습니다!\n\n"
			"#FF8000 RCM 진입 방법#: #EFEFEF %s#", gui_pv_btn(GUI_PV_BTN_0)
		);
	} else {
		s_printf(mbox_txt,
			"#008EED 상태 메시지#\n\n"
			"#FFBA00 안내#: 자동 RCM이 #FF8000 비활성화# 되었습니다!\n\n"
			"#FF8000 RCM 진입 방법#: 지그 삽입 후, %s + #EFEFEF %s#",
			gui_pv_btn(GUI_PV_BTN_4), gui_pv_btn(GUI_PV_BTN_0)
		);
	}

	lv_mbox_set_text(mbox, mbox_txt);

	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	if (enabled)
		lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);
	else
		lv_btn_set_state(btn, LV_BTN_STATE_REL);
	nyx_generic_onoff_toggle(btn);

	return LV_RES_OK;
}

static lv_res_t _create_mbox_hid(usb_ctxt_t *usbs)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map_dis[] = { "\251", "\262닫기", "\251", "" };
	static const char *mbox_btn_map[] = { "\251", "\222닫기", "\251", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);

	char *txt_buf = malloc(SZ_4K);

	s_printf(txt_buf, "#FF8000 HID 에뮬레이션#\n\n#C7EA46 기기#: ");

	if (usbs->type == USB_HID_GAMEPAD)
		strcat(txt_buf, "컨트롤러");
	else
		strcat(txt_buf, "터치패드");

	lv_mbox_set_text(mbox, txt_buf);
	free(txt_buf);

	lv_obj_t *lbl_status = lv_label_create(mbox, NULL);
	lv_label_set_recolor(lbl_status, true);
	lv_label_set_text(lbl_status, " ");
	usbs->label = (void *)lbl_status;

	lv_obj_t *lbl_tip = lv_label_create(mbox, NULL);
	lv_label_set_recolor(lbl_tip, true);
	lv_label_set_static_text(lbl_tip, "#FFBA00 안내#: 연결 종료하려면 #C7EA46 Ⓙ# + #C7EA46 Ⓗ# 을 입력하거나, 케이블을 제거하세요.");
	lv_obj_set_style(lbl_tip, &hint_small_style);

	lv_mbox_add_btns(mbox, mbox_btn_map_dis, nyx_mbox_action);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	usb_device_gadget_hid(usbs);

	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);

	return LV_RES_OK;
}

static lv_res_t _create_mbox_ums(usb_ctxt_t *usbs)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map_dis[] = { "\251", "\262닫기", "\251", "" };
	static const char *mbox_btn_map[] = { "\251", "\222닫기", "\251", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);

	char *txt_buf = malloc(SZ_4K);

	s_printf(txt_buf, "#008EED USB 저장 장치#\n\n#C7EA46 드라이브#: ");

	if (usbs->type == MMC_SD)
	{
		switch (usbs->partition)
		{
		case 0:
			strcat(txt_buf, "Micro SD 카드");
			break;
		case EMMC_GPP + 1:
			strcat(txt_buf, "에뮤낸드 GPP");
			break;
		case EMMC_BOOT0 + 1:
			strcat(txt_buf, "에뮤낸드 BOOT0");
			break;
		case EMMC_BOOT1 + 1:
			strcat(txt_buf, "에뮤낸드 BOOT1");
			break;
		}
	}
	else
	{
		switch (usbs->partition)
		{
		case EMMC_GPP + 1:
			strcat(txt_buf, "시스낸드 GPP");
			break;
		case EMMC_BOOT0 + 1:
			strcat(txt_buf, "시스낸드 BOOT0");
			break;
		case EMMC_BOOT1 + 1:
			strcat(txt_buf, "시스낸드 BOOT1");
			break;
		}
	}

	lv_mbox_set_text(mbox, txt_buf);
	free(txt_buf);

	lv_obj_t *lbl_status = lv_label_create(mbox, NULL);
	lv_label_set_recolor(lbl_status, true);
	lv_label_set_text(lbl_status, " ");
	usbs->label = (void *)lbl_status;

	lv_obj_t *lbl_tip = lv_label_create(mbox, NULL);
	lv_label_set_recolor(lbl_tip, true);
	if (!usbs->ro)
	{
		if (usbs->type == MMC_SD)
		{
			lv_label_set_static_text(lbl_tip,
				"#FFBA00 안내#: 안전하게 종료하기 위해, PC에서 #C7EA46 꺼내기#를 선택하세요.\n"
				"           #FF8000 작업 중에 케이블을 제거하지 마세요!#");
		}
		else
		{
			lv_label_set_static_text(lbl_tip,
				"#FFBA00 안내#: 안전하게 종료하기 위해, PC에서 #C7EA46 꺼내기#를 선택하세요.\n"
				"   #FF8000 케이블이 연결되어 있지 않다면 제거하여 종료하세요!#");
		}
	}
	else
	{
		lv_label_set_static_text(lbl_tip,
			"      #FFBA00 안내#: 파일 전송 작업이 완료되었습니다.\n"
			"PC에서 #C7EA46 꺼내기#를 선택하거나, 케이블을 제거하세요!#");
	}
	lv_obj_set_style(lbl_tip, &hint_small_style);

	lv_mbox_add_btns(mbox, mbox_btn_map_dis, nyx_mbox_action);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	// Dim backlight.
	display_backlight_brightness(20, 1000);

	usb_device_gadget_ums(usbs);

	// Restore backlight.
	display_backlight_brightness(h_cfg.backlight - 20, 1000);

	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);

	ums_mbox = dark_bg;

	return LV_RES_OK;
}

static lv_res_t _create_mbox_ums_error(int error)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);

	switch (error)
	{
	case 1:
		lv_mbox_set_text(mbox, "#008EED USB 저장 장치#\n\n#FFBA00 안내#: #FF8000 SD 카드를 읽을 수 없습니다!#");
		break;
	case 2:
		lv_mbox_set_text(mbox, "#008EED USB 저장 장치#\n\n#FFBA00 안내#: #FF8000 활성화된 에뮤낸드를 찾을 수 없습니다!#");
		break;
	case 3:
		lv_mbox_set_text(mbox, "#008EED USB 저장 장치#\n\n#FFBA00 안내#: #FF8000 파티션 기반 에뮤낸드가 아닙니다!#");
		break;
	}

	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}

static void usb_gadget_set_text(void *lbl, const char *text)
{
	lv_label_set_text((lv_obj_t *)lbl, text);
	manual_system_maintenance(true);
}

lv_res_t _action_hid_jc(lv_obj_t *btn)
{
	// Reduce BPMP, RAM and backlight and power off SDMMC1 to conserve power.
	sd_end();
	minerva_change_freq(FREQ_800);
	bpmp_clk_rate_relaxed(true);
	display_backlight_brightness(10, 1000);

	usb_ctxt_t usbs;
	usbs.type = USB_HID_GAMEPAD;
	usbs.system_maintenance = &manual_system_maintenance;
	usbs.set_text = &usb_gadget_set_text;

	_create_mbox_hid(&usbs);

	// Restore BPMP, RAM and backlight.
	minerva_change_freq(FREQ_1600);
	bpmp_clk_rate_relaxed(false);
	display_backlight_brightness(h_cfg.backlight - 20, 1000);

	return LV_RES_OK;
}

/*
static lv_res_t _action_hid_touch(lv_obj_t *btn)
{
	// Reduce BPMP, RAM and backlight and power off SDMMC1 to conserve power.
	sd_end();
	minerva_change_freq(FREQ_800);
	bpmp_clk_rate_relaxed(true);
	display_backlight_brightness(10, 1000);

	usb_ctxt_t usbs;
	usbs.type = USB_HID_TOUCHPAD;
	usbs.system_maintenance = &manual_system_maintenance;
	usbs.set_text = &usb_gadget_set_text;

	_create_mbox_hid(&usbs);

	// Restore BPMP, RAM and backlight.
	minerva_change_freq(FREQ_1600);
	bpmp_clk_rate_relaxed(false);
	display_backlight_brightness(h_cfg.backlight - 20, 1000);

	return LV_RES_OK;
}
*/

static bool usb_msc_emmc_read_only;
lv_res_t action_ums_sd(lv_obj_t *btn)
{
	usb_ctxt_t usbs;
	usbs.type = MMC_SD;
	usbs.partition = 0;
	usbs.offset = 0;
	usbs.sectors = 0;
	usbs.ro = 0;
	usbs.system_maintenance = &manual_system_maintenance;
	usbs.set_text = &usb_gadget_set_text;

	_create_mbox_ums(&usbs);

	return LV_RES_OK;
}

lv_res_t _action_ums_emmc_boot0(lv_obj_t *btn)
{
	if (!nyx_emmc_check_battery_enough())
		return LV_RES_OK;

	usb_ctxt_t usbs;
	usbs.type = MMC_EMMC;
	usbs.partition = EMMC_BOOT0 + 1;
	usbs.offset = 0;
	usbs.sectors = 0;
	usbs.ro = usb_msc_emmc_read_only;
	usbs.system_maintenance = &manual_system_maintenance;
	usbs.set_text = &usb_gadget_set_text;

	_create_mbox_ums(&usbs);

	return LV_RES_OK;
}

lv_res_t _action_ums_emmc_boot1(lv_obj_t *btn)
{
	if (!nyx_emmc_check_battery_enough())
		return LV_RES_OK;

	usb_ctxt_t usbs;
	usbs.type = MMC_EMMC;
	usbs.partition = EMMC_BOOT1 + 1;
	usbs.offset = 0;
	usbs.sectors = 0;
	usbs.ro = usb_msc_emmc_read_only;
	usbs.system_maintenance = &manual_system_maintenance;
	usbs.set_text = &usb_gadget_set_text;

	_create_mbox_ums(&usbs);

	return LV_RES_OK;
}

lv_res_t _action_ums_emmc_gpp(lv_obj_t *btn)
{
	if (!nyx_emmc_check_battery_enough())
		return LV_RES_OK;

	usb_ctxt_t usbs;
	usbs.type = MMC_EMMC;
	usbs.partition = EMMC_GPP + 1;
	usbs.offset = 0;
	usbs.sectors = 0;
	usbs.ro = usb_msc_emmc_read_only;
	usbs.system_maintenance = &manual_system_maintenance;
	usbs.set_text = &usb_gadget_set_text;

	_create_mbox_ums(&usbs);

	return LV_RES_OK;
}

lv_res_t _action_ums_emuemmc_boot0(lv_obj_t *btn)
{
	if (!nyx_emmc_check_battery_enough())
		return LV_RES_OK;

	usb_ctxt_t usbs;

	int error = sd_mount();
	if (!error)
	{
		emummc_cfg_t emu_info;
		load_emummc_cfg(&emu_info);

		error = 2;
		if (emu_info.enabled)
		{
			error = 3;
			if (emu_info.sector)
			{
				error = 0;
				usbs.offset = emu_info.sector;
			}
		}

		if (emu_info.path)
			free(emu_info.path);
		if (emu_info.nintendo_path)
			free(emu_info.nintendo_path);
	}
	sd_unmount();

	if (error)
		_create_mbox_ums_error(error);
	else
	{
		usbs.type = MMC_SD;
		usbs.partition = EMMC_BOOT0 + 1;
		usbs.sectors = 0x2000; // Forced 4MB.
		usbs.ro = usb_msc_emmc_read_only;
		usbs.system_maintenance = &manual_system_maintenance;
		usbs.set_text = &usb_gadget_set_text;
		_create_mbox_ums(&usbs);
	}

	return LV_RES_OK;
}

lv_res_t _action_ums_emuemmc_boot1(lv_obj_t *btn)
{
	if (!nyx_emmc_check_battery_enough())
		return LV_RES_OK;

	usb_ctxt_t usbs;

	int error = sd_mount();
	if (!error)
	{
		emummc_cfg_t emu_info;
		load_emummc_cfg(&emu_info);

		error = 2;
		if (emu_info.enabled)
		{
			error = 3;
			if (emu_info.sector)
			{
				error = 0;
				usbs.offset = emu_info.sector + 0x2000;
			}
		}

		if (emu_info.path)
			free(emu_info.path);
		if (emu_info.nintendo_path)
			free(emu_info.nintendo_path);
	}
	sd_unmount();

	if (error)
		_create_mbox_ums_error(error);
	else
	{
		usbs.type = MMC_SD;
		usbs.partition = EMMC_BOOT1 + 1;
		usbs.sectors = 0x2000; // Forced 4MB.
		usbs.ro = usb_msc_emmc_read_only;
		usbs.system_maintenance = &manual_system_maintenance;
		usbs.set_text = &usb_gadget_set_text;
		_create_mbox_ums(&usbs);
	}

	return LV_RES_OK;
}

lv_res_t _action_ums_emuemmc_gpp(lv_obj_t *btn)
{
	if (!nyx_emmc_check_battery_enough())
		return LV_RES_OK;

	usb_ctxt_t usbs;

	int error = sd_mount();
	if (!error)
	{
		emummc_cfg_t emu_info;
		load_emummc_cfg(&emu_info);

		error = 2;
		if (emu_info.enabled)
		{
			error = 3;
			if (emu_info.sector)
			{
				error = 1;
				usbs.offset = emu_info.sector + 0x4000;

				u8 *gpt = malloc(SD_BLOCKSIZE);
				if (!sdmmc_storage_read(&sd_storage, usbs.offset + 1, 1, gpt))
				{
					if (!memcmp(gpt, "EFI PART", 8))
					{
						error = 0;
						usbs.sectors = *(u32 *)(gpt + 0x20) + 1; // Backup LBA + 1.
					}
				}
			}
		}

		if (emu_info.path)
			free(emu_info.path);
		if (emu_info.nintendo_path)
			free(emu_info.nintendo_path);
	}
	sd_unmount();

	if (error)
		_create_mbox_ums_error(error);
	else
	{
		usbs.type = MMC_SD;
		usbs.partition = EMMC_GPP + 1;
		usbs.ro = usb_msc_emmc_read_only;
		usbs.system_maintenance = &manual_system_maintenance;
		usbs.set_text = &usb_gadget_set_text;
		_create_mbox_ums(&usbs);
	}

	return LV_RES_OK;
}

void nyx_run_ums(void *param)
{
	u32 *cfg = (u32 *)param;

	u8 type = (*cfg) >> 24;
	*cfg = *cfg & (~NYX_CFG_EXTRA);

	// Disable read only flag.
	usb_msc_emmc_read_only = false;

	switch (type)
	{
	case NYX_UMS_SD_CARD:
		action_ums_sd(NULL);
		break;
	case NYX_UMS_EMMC_BOOT0:
		_action_ums_emmc_boot0(NULL);
		break;
	case NYX_UMS_EMMC_BOOT1:
		_action_ums_emmc_boot1(NULL);
		break;
	case NYX_UMS_EMMC_GPP:
		_action_ums_emmc_gpp(NULL);
		break;
	case NYX_UMS_EMUMMC_BOOT0:
		_action_ums_emuemmc_boot0(NULL);
		break;
	case NYX_UMS_EMUMMC_BOOT1:
		_action_ums_emuemmc_boot1(NULL);
		break;
	case NYX_UMS_EMUMMC_GPP:
		_action_ums_emuemmc_gpp(NULL);
		break;
	}
}

lv_res_t _emmc_read_only_toggle(lv_obj_t *btn)
{
	nyx_generic_onoff_toggle(btn);

	usb_msc_emmc_read_only = lv_btn_get_state(btn) & LV_BTN_STATE_TGL_REL ? 1 : 0;

	return LV_RES_OK;
}

static int _fix_attributes(lv_obj_t *lb_val, char *path, u32 *total)
{
	FRESULT res;
	DIR dir;
	u32 dirLength = 0;
	static FILINFO fno;

	// Open directory.
	res = f_opendir(&dir, path);
	if (res != FR_OK)
		return res;

	dirLength = strlen(path);

	// Hard limit path to 1024 characters. Do not result to error.
	if (dirLength > 1024)
	{
		total[2]++;
		goto out;
	}

	for (;;)
	{
		// Clear file or folder path.
		path[dirLength] = 0;

		// Read a directory item.
		res = f_readdir(&dir, &fno);

		// Break on error or end of dir.
		if (res != FR_OK || fno.fname[0] == 0)
			break;

		// Set new directory or file.
		memcpy(&path[dirLength], "/", 1);
		strcpy(&path[dirLength + 1], fno.fname);

		// Is it a directory?
		if (fno.fattrib & AM_DIR)
		{
			// Check if it's a HOS single file folder.
			strcat(path, "/00");
			bool is_hos_special = !f_stat(path, NULL);
			path[strlen(path) - 3] = 0;

			// Set archive bit to HOS single file folders.
			if (is_hos_special)
			{
				if (!(fno.fattrib & AM_ARC))
				{
					if (!f_chmod(path, AM_ARC, AM_ARC))
						total[0]++;
					else
						total[3]++;
				}
			}
			else if (fno.fattrib & AM_ARC) // If not, clear the archive bit.
			{
				if (!f_chmod(path, 0, AM_ARC))
					total[1]++;
				else
					total[3]++;
			}

			lv_label_set_text(lb_val, path);
			manual_system_maintenance(true);

			// Enter the directory.
			res = _fix_attributes(lb_val, path, total);
			if (res != FR_OK)
				break;
		}
	}

out:
	f_closedir(&dir);

	return res;
}

//============================
//  ASAP: Moved (gui_info.c)
//============================
lv_res_t _create_window_unset_abit_tool(lv_obj_t *btn)
{
	lv_obj_t *win = nyx_create_standard_window(SYMBOL_DIRECTORY"  아카이브 비트 재설정", NULL);

	// Disable buttons.
	nyx_window_toggle_buttons(win, true);

	lv_obj_t *desc = lv_cont_create(win, NULL);
	lv_obj_set_size(desc, LV_HOR_RES * 10 / 11, LV_VER_RES - (LV_DPI * 11 / 7) * 4);

	lv_obj_t * lb_desc = lv_label_create(desc, NULL);
	lv_label_set_long_mode(lb_desc, LV_LABEL_LONG_BREAK);
	lv_label_set_recolor(lb_desc, true);

	if (sd_mount())
	{
		lv_label_set_text(lb_desc, "#FFBA00 SD 카드 초기화 실패!#");
		lv_obj_set_width(lb_desc, lv_obj_get_width(desc));
	}
	else
	{
		lv_label_set_text(lb_desc, "#00DDFF SD 카드의 모든 폴더와 파일을 검사합니다!#\n잠시만 기다려주세요...");
		lv_obj_set_width(lb_desc, lv_obj_get_width(desc));

		lv_obj_t *val = lv_cont_create(win, NULL);
		lv_obj_set_size(val, LV_HOR_RES * 10 / 11, LV_VER_RES - (LV_DPI * 11 / 7) * 4);

		lv_obj_t * lb_val = lv_label_create(val, lb_desc);

		char *path = malloc(0x1000);
		path[0] = 0;

		lv_label_set_text(lb_val, "");
		lv_obj_set_width(lb_val, lv_obj_get_width(val));
		lv_obj_align(val, desc, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

		u32 total[4] = { 0 };
		_fix_attributes(lb_val, path, total);

		sd_unmount();

		lv_obj_t *desc2 = lv_cont_create(win, NULL);
		lv_obj_set_size(desc2, LV_HOR_RES * 10 / 11, LV_VER_RES - (LV_DPI * 11 / 7) * 4);
		lv_obj_t * lb_desc2 = lv_label_create(desc2, lb_desc);

		char *txt_buf = (char *)malloc(0x500);

		if (!total[0] && !total[1])
			s_printf(txt_buf, "#96FF00 검사 완료했으나, 재설정이 필요하지 않습니다.#");
		else
			s_printf(txt_buf, "#96FF00 검사가 완료되었습니다!#\n"
							  "#00DDFF 설정 해제#: #FF8000 %d#, #00DDFF 재설정#: #FF8000 %d#",
							  total[1], total[0]);

		// Check errors.
		if (total[2] || total[3])
		{
			s_printf(txt_buf, "\n\n#FF8000 접근 폴더#: #FF8000 %d#, #FF8000 재설정#: #FF8000 %d#\n"
							  "#FFBA00 파일 시스템에 오류가 없는지 확인하세요.#",
							  total[2], total[3]);
		}

		lv_label_set_text(lb_desc2, txt_buf);
		lv_obj_set_width(lb_desc2, lv_obj_get_width(desc2));
		lv_obj_align(desc2, val, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 0);

		free(path);
	}

	// Enable buttons.
	nyx_window_toggle_buttons(win, false);

	return LV_RES_OK;
}

lv_res_t _create_mbox_fix_touchscreen(lv_obj_t *btn)
{
	int res = 1;
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);

	char *txt_buf = malloc(SZ_16K);

	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 6);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, LV_DPI / -1.5);
	lv_obj_set_top(mbox, true);

	s_printf(txt_buf,
		"#008EED 터치 패널 보정\n\n"
		"#FF8000 경고#: 반드시 문제가 있는 경우에만 실행하세요!\n\n"
		"#FF8000 계속#: #EFEFEF %s#   #FF8000 중단#: #EFEFEF %s# 또는 #EFEFEF %s#",
		gui_pv_btn(GUI_PV_BTN_0), gui_pv_btn(GUI_PV_BTN_1), gui_pv_btn(GUI_PV_BTN_2)
	);

	lv_mbox_set_text(mbox, txt_buf);
	manual_system_maintenance(true);

	if (!(btn_wait() & BTN_POWER))
		goto out;

	manual_system_maintenance(true);
	lv_mbox_set_text(mbox, txt_buf);

	u32 seconds = 5;
	while (seconds)
	{
		s_printf(txt_buf, "#FF8000 경고#: %d초 후 작업이 시작됩니다, 화면을 터치하지 마세요!", seconds);
		lv_mbox_set_text(mbox, txt_buf);
		manual_system_maintenance(true);
		msleep(1000);
		seconds--;
	}

	u8 err[2];
	if (touch_panel_ito_test(err))
		goto ito_failed;

	if (!err[0] && !err[1])
	{
		res = touch_execute_autotune();
		if (!res)
			goto out;
	}
	else
	{
		touch_sense_enable();

		s_printf(txt_buf, "#FFFF00 ITO 테스트: ");
		switch (err[0])
		{
		case ITO_FORCE_OPEN:
			strcat(txt_buf, "Force 회로 단선");
			break;
		case ITO_SENSE_OPEN:
			strcat(txt_buf, "Sense 회로 단선");
			break;
		case ITO_FORCE_SHRT_GND:
			strcat(txt_buf, "Force 회로와 GND 단락");
			break;
		case ITO_SENSE_SHRT_GND:
			strcat(txt_buf, "Sense 회로와 GND 단락");
			break;
		case ITO_FORCE_SHRT_VCM:
			strcat(txt_buf, "Force 회로와 VDD 단락");
			break;
		case ITO_SENSE_SHRT_VCM:
			strcat(txt_buf, "Sense 회로와 VDD 단락");
			break;
		case ITO_FORCE_SHRT_FORCE:
			strcat(txt_buf, "Force 회로끼리 단락");
			break;
		case ITO_SENSE_SHRT_SENSE:
			strcat(txt_buf, "Sense 회로끼리 단락");
			break;
		case ITO_F2E_SENSE:
			strcat(txt_buf, "Force, Sense 회로 단락");
			break;
		case ITO_FPC_FORCE_OPEN:
			strcat(txt_buf, "케이블 Force 회로 단선");
			break;
		case ITO_FPC_SENSE_OPEN:
			strcat(txt_buf, "케이블 Sense 회로 단선");
			break;
		default:
			strcat(txt_buf, "해독 불가");
			break;

		}
		s_printf(txt_buf + strlen(txt_buf), " (%d), 채널: %d#\n\n", err[0], err[1]);
		strcat(txt_buf, "#FFBA00 터치 패널 보정에 실패했습니다!");
		lv_mbox_set_text(mbox, txt_buf);
		goto out2;
	}

ito_failed:
	touch_sense_enable();

out:
	if (!res)
		lv_mbox_set_text(mbox, "#C7EA46 터치 패널 보정이 완료되었습니다!");
	else
		lv_mbox_set_text(mbox, "#FFBA00 터치 패널 보정에 실패했습니다!");

out2:
	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);

	free(txt_buf);

	return LV_RES_OK;
}

lv_res_t _create_window_dump_pk12_tool(lv_obj_t *btn)
{
	lv_obj_t *win = nyx_create_standard_window(SYMBOL_MODULES"  PACKAGE1, PACKAGE2 백업", NULL);

	//===================
	//  ASAP: eMMC S/N.
	//===================
	char *sn = emmcsn_path_impl(NULL, NULL, NULL, NULL);
	//===================

	// Disable buttons.
	nyx_window_toggle_buttons(win, true);

	lv_obj_t *desc = lv_cont_create(win, NULL);
	lv_obj_set_size(desc, LV_HOR_RES * 10 / 11, LV_VER_RES - (LV_DPI * 12 / 8));

	lv_obj_t *lb_desc = lv_label_create(desc, NULL);
	lv_obj_set_style(lb_desc, &monospace_text);
	lv_label_set_long_mode(lb_desc, LV_LABEL_LONG_BREAK);
	lv_label_set_recolor(lb_desc, true);
	lv_obj_set_width(lb_desc, lv_obj_get_width(desc) / 2);

	lv_obj_t *lb_desc2 = lv_label_create(desc, NULL);
	lv_obj_set_style(lb_desc2, &monospace_text);
	lv_label_set_long_mode(lb_desc2, LV_LABEL_LONG_BREAK);
	lv_label_set_recolor(lb_desc2, true);
	lv_obj_set_width(lb_desc2, lv_obj_get_width(desc) / 2);
	lv_label_set_text(lb_desc2, " ");

	lv_obj_align(lb_desc, desc, LV_ALIGN_IN_TOP_LEFT, 20, 20);
	lv_obj_align(lb_desc2, lb_desc, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI / 3, 0);

	if (sd_mount())
	{
		lv_label_set_text(lb_desc, "#FFBA00 SD 카 드  초 기 화  실 패 !#");

		goto out_end;
	}

	char path[128];

	u8 mkey = 0;
	u8 *pkg1 = (u8 *)zalloc(SZ_256K);
	u8 *warmboot = (u8 *)zalloc(SZ_256K);
	u8 *secmon = (u8 *)zalloc(SZ_256K);
	u8 *loader = (u8 *)zalloc(SZ_256K);
	u8 *pkg2   = (u8 *)zalloc(SZ_8M);

	char *txt_buf  = (char *)malloc(SZ_16K);

	if (emmc_initialize(false))
	{
		lv_label_set_text(lb_desc, "#FFBA00 eMMC 초 기 화  실 패 !#");

		goto out_free;
	}

	char *bct_paths[2] = {
		"/pkg/main",
		"/pkg/safe"
	};

	char *pkg1_paths[2] = {
		"/pkg/main/pkg1",
		"/pkg/safe/pkg1"
	};

	char *pkg2_partitions[2] = {
		"BCPKG2-1-Normal-Main",
		"BCPKG2-3-SafeMode-Main"
	};

	char *pkg2_paths[2] = {
		"/pkg/main/pkg2",
		"/pkg/safe/pkg2"
	};

	char *pkg2ini_paths[2] = {
		"/pkg/main/pkg2/ini",
		"/pkg/safe/pkg2/ini"
	};

	// Create main directories.
	emmcsn_path_impl(path, "/pkg", "", &emmc_storage);
	emmcsn_path_impl(path, "/pkg/main", "", &emmc_storage);
	emmcsn_path_impl(path, "/pkg/safe", "", &emmc_storage);

	// Parse eMMC GPT.
	emmc_set_partition(EMMC_GPP);
	LIST_INIT(gpt);
	emmc_gpt_parse(&gpt);

	lv_obj_t *lb_log = lb_desc;
	for (u32 idx = 0; idx < 2; idx++)
	{
		if (idx)
			lb_log = lb_desc2;

		// Read package1.
		char *build_date = malloc(32);
		u32   pk1_offset = h_cfg.t210b01 ? sizeof(bl_hdr_t210b01_t) : 0; // Skip T210B01 OEM header.
		emmc_set_partition(!idx ? EMMC_BOOT0 : EMMC_BOOT1);
		sdmmc_storage_read(&emmc_storage,
			!idx ? PKG1_BOOTLOADER_MAIN_OFFSET : PKG1_BOOTLOADER_SAFE_OFFSET, PKG1_BOOTLOADER_SIZE / EMMC_BLOCKSIZE, pkg1);

		const pkg1_id_t *pkg1_id = pkg1_identify(pkg1 + pk1_offset, build_date);

		s_printf(txt_buf, "#00DDFF %s pkg1 ('%s') 을  찾 았 습 니 다.#\n\n", !idx ? "Main" : "Safe", build_date);
		lv_label_set_text(lb_log, txt_buf);
		manual_system_maintenance(true);
		free(build_date);

		// Dump package1 in its encrypted state.
		emmcsn_path_impl(path, pkg1_paths[idx], "pkg1_enc.bin", &emmc_storage);
		bool res = sd_save_to_file(pkg1, PKG1_BOOTLOADER_SIZE, path);

		// Exit if unknown.
		if (!pkg1_id)
		{
			strcat(txt_buf, " #FFBA00 지 원 하 지  않 는  펌 웨 어 가  설 치 되 어 있 습 니 다 !#");
			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);

			if (!res)
			{
				strcat(txt_buf, "\n 암 호 화 된 Package1 이 pkg1_enc.bin 로  추 출 되 었 습 니 다.");
				lv_label_set_text(lb_log, txt_buf);
				manual_system_maintenance(true);
			}

			goto out;
		}

		mkey = pkg1_id->mkey;

		tsec_ctxt_t tsec_ctxt = {0};
		tsec_ctxt.fw = (void *)(pkg1 + pkg1_id->tsec_off);
		tsec_ctxt.pkg1 = (void *)pkg1;
		tsec_ctxt.pkg11_off = pkg1_id->pkg11_off;

		// Read the correct eks for older HOS versions.
		const u32 eks_size = sizeof(pkg1_eks_t);
		pkg1_eks_t *eks = (pkg1_eks_t *)malloc(eks_size);
		emmc_set_partition(EMMC_BOOT0);
		sdmmc_storage_read(&emmc_storage, PKG1_HOS_EKS_OFFSET + (mkey * eks_size) / EMMC_BLOCKSIZE,
										  eks_size / EMMC_BLOCKSIZE, eks);

		// Generate keys.
		hos_keygen(eks, mkey, &tsec_ctxt);
		free(eks);

		// Decrypt.
		if (h_cfg.t210b01 || mkey <= HOS_MKEY_VER_600)
		{
			if (!pkg1_decrypt(pkg1_id, pkg1))
			{
				strcat(txt_buf, " #FFBA00 Package1 복 호 화 에  실 패 했 습 니 다 !#\n");
				if (h_cfg.t210b01)
					strcat(txt_buf, " #FFBA00 BEK 가  누 락 되 어 있 지  않 습 니 까 ?#\n");
				lv_label_set_text(lb_log, txt_buf);
				goto out;
			}
		}

		// Dump the BCTs from blocks 2/3 (backup) which are normally valid.
		static const u32 BCT_SIZE = 0x2800;
		static const u32 BLK_SIZE = SZ_16K / EMMC_BLOCKSIZE;
		u8 *bct = (u8 *)zalloc(BCT_SIZE);
		sdmmc_storage_read(&emmc_storage, BLK_SIZE * 2 + BLK_SIZE * idx, BCT_SIZE / EMMC_BLOCKSIZE, bct);
		emmcsn_path_impl(path, bct_paths[idx], "bct.bin", &emmc_storage);
		if (sd_save_to_file(bct, 0x2800, path))
			goto out;
		if (h_cfg.t210b01)
		{
			se_aes_iv_clear(13);
			se_aes_crypt_cbc(13, DECRYPT, bct + 0x480, bct + 0x480, BCT_SIZE - 0x480);
			emmcsn_path_impl(path, bct_paths[idx], "bct_decr.bin", &emmc_storage);
			if (sd_save_to_file(bct, 0x2800, path))
				goto out;
		}

		// Dump package1.1 contents.
		if (h_cfg.t210b01 || mkey <= HOS_MKEY_VER_620)
		{
			pkg1_unpack(warmboot, secmon, loader, pkg1_id, pkg1 + pk1_offset);
			pk11_hdr_t *hdr_pk11 = (pk11_hdr_t *)(pkg1 + pk1_offset + pkg1_id->pkg11_off + 0x20);

			// Display info.
			s_printf(txt_buf + strlen(txt_buf),
				"#C7EA46  NX-Bootloader  크 기# :  0x%05X\n"
				"#C7EA46  Secure Monitor 크 기# :  0x%05X\n"
				"#C7EA46  Warmboot 크 기#       :  0x%05X\n\n",
				hdr_pk11->ldr_size, hdr_pk11->sm_size, hdr_pk11->wb_size);

			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);

			// Dump package1.1.
			emmcsn_path_impl(path, pkg1_paths[idx], "pkg1_decr.bin", &emmc_storage);
			if (sd_save_to_file(pkg1, SZ_256K, path))
				goto out;
			strcat(txt_buf, " pkg1               >  #00FFCC pkg1_decr.bin#\n");
			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);

			// Dump nxbootloader.
			emmcsn_path_impl(path, pkg1_paths[idx], "nxloader.bin", &emmc_storage);
			if (sd_save_to_file(loader, hdr_pk11->ldr_size, path))
				goto out;
			strcat(txt_buf, " NX-Bootloader      >  #00FFCC nxloader.bin#\n");
			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);

			// Dump secmon.
			emmcsn_path_impl(path, pkg1_paths[idx], "secmon.bin", &emmc_storage);
			if (sd_save_to_file(secmon, hdr_pk11->sm_size, path))
				goto out;
			strcat(txt_buf, " Secure Monitor     >  #00FFCC secmon.bin#\n");
			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);

			// Dump warmboot.
			emmcsn_path_impl(path, pkg1_paths[idx], "warmboot.bin", &emmc_storage);
			if (sd_save_to_file(warmboot, hdr_pk11->wb_size, path))
				goto out;
			// If T210B01, save a copy of decrypted warmboot binary also.
			if (h_cfg.t210b01)
			{

				se_aes_iv_clear(13);
				se_aes_crypt_cbc(13, DECRYPT, warmboot + 0x330, warmboot + 0x330, hdr_pk11->wb_size - 0x330);
				emmcsn_path_impl(path, pkg1_paths[idx], "warmboot_dec.bin", &emmc_storage);
				if (sd_save_to_file(warmboot, hdr_pk11->wb_size, path))
					goto out;
			}
			strcat(txt_buf, " Warmboot           >  #00FFCC warmboot.bin#\n\n");
			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);
		}

		// Find and dump package2 partition.
		emmc_set_partition(EMMC_GPP);
		emmc_part_t *pkg2_part = emmc_part_find(&gpt, pkg2_partitions[idx]);
		if (!pkg2_part)
			goto out;

		// Read in package2 header and get package2 real size.
		static const u32 PKG2_OFFSET = 0x4000;
		u8 *tmp = (u8 *)malloc(EMMC_BLOCKSIZE);
		emmc_part_read(pkg2_part, PKG2_OFFSET / EMMC_BLOCKSIZE, 1, tmp);
		u32 *hdr_pkg2_raw = (u32 *)(tmp + 0x100);
		u32 pkg2_size = hdr_pkg2_raw[0] ^ hdr_pkg2_raw[2] ^ hdr_pkg2_raw[3];
		free(tmp);

		// Read in package2.
		u32 pkg2_size_aligned = ALIGN(pkg2_size, EMMC_BLOCKSIZE);
		emmc_part_read(pkg2_part, PKG2_OFFSET / EMMC_BLOCKSIZE, pkg2_size_aligned / EMMC_BLOCKSIZE, pkg2);

		// Dump encrypted package2.
		emmcsn_path_impl(path, pkg2_paths[idx], "pkg2_encr.bin", &emmc_storage);
		res = sd_save_to_file(pkg2, pkg2_size, path);

		// Decrypt package2 and parse KIP1 blobs in INI1 section.
		pkg2_hdr_t *pkg2_hdr = pkg2_decrypt(pkg2, mkey);
		if (!pkg2_hdr)
		{
			strcat(txt_buf, " #FFBA00 Package2 복 호 화 에  실 패 했 습 니 다 !#");
			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);

			if (!res)
			{
				strcat(txt_buf, "\n 암 호 화 pkg2         >  #00FFCC pkg2_encr.bin#\n");
				lv_label_set_text(lb_log, txt_buf);
				manual_system_maintenance(true);
			}

			// Clear EKS slot, in case something went wrong with tsec keygen.
			hos_eks_clear(mkey);

			goto out;
		}

		// Display info.
		s_printf(txt_buf + strlen(txt_buf),
			" #C7EA46 Kernel 크 기#         :  0x%06X\n"
			" #C7EA46 INI1   크 기#         :  0x%06X\n\n",
			pkg2_hdr->sec_size[PKG2_SEC_KERNEL], pkg2_hdr->sec_size[PKG2_SEC_INI1]);

		lv_label_set_text(lb_log, txt_buf);
		manual_system_maintenance(true);

		// Dump pkg2.1.
		emmcsn_path_impl(path, pkg2_paths[idx], "pkg2_decr.bin", &emmc_storage);
		if (sd_save_to_file(pkg2, pkg2_hdr->sec_size[PKG2_SEC_KERNEL] + pkg2_hdr->sec_size[PKG2_SEC_INI1], path))
			goto out;
		strcat(txt_buf, " pkg2               >  #00FFCC pkg2_decr.bin#\n");
		lv_label_set_text(lb_log, txt_buf);
		manual_system_maintenance(true);

		// Dump kernel.
		emmcsn_path_impl(path, pkg2_paths[idx], "kernel.bin", &emmc_storage);
		if (sd_save_to_file(pkg2_hdr->data, pkg2_hdr->sec_size[PKG2_SEC_KERNEL], path))
			goto out;
		strcat(txt_buf, " Kernel             >  #00FFCC kernel.bin#\n");
		lv_label_set_text(lb_log, txt_buf);
		manual_system_maintenance(true);

		// Dump INI1.
		u32 ini1_off  = pkg2_hdr->sec_size[PKG2_SEC_KERNEL];
		u32 ini1_size = pkg2_hdr->sec_size[PKG2_SEC_INI1];
		if (!ini1_size)
		{
			pkg2_get_newkern_info(pkg2_hdr->data);
			ini1_off  = pkg2_newkern_ini1_start;
			ini1_size = pkg2_newkern_ini1_end - pkg2_newkern_ini1_start;
		}

		if (!ini1_off)
		{
			strcat(txt_buf, "#FFBA00  INI1 및 kips 백 업  실 패 !#\n");
			goto out;
		}

		pkg2_ini1_t *ini1 = (pkg2_ini1_t *)(pkg2_hdr->data + ini1_off);
		emmcsn_path_impl(path, pkg2_paths[idx], "ini1.bin", &emmc_storage);
		if (sd_save_to_file(ini1, ini1_size, path))
			goto out;

		strcat(txt_buf, " INI1               >  #00FFCC ini1.bin#\n");
		lv_label_set_text(lb_log, txt_buf);
		manual_system_maintenance(true);

		char filename[32];
		u8 *ptr = (u8 *)ini1;
		ptr += sizeof(pkg2_ini1_t);

		// Dump all kips.
		for (u32 i = 0; i < ini1->num_procs; i++)
		{
			pkg2_kip1_t *kip1 = (pkg2_kip1_t *)ptr;
			u32 kip1_size = pkg2_calc_kip1_size(kip1);
			char *kip_name = kip1->name;

			// Check if FS supports exFAT.
			if (!strcmp("FS", kip_name))
			{
				u8 *ro_data = malloc(SZ_4M);
				u32 offset      = (kip1->flags & BIT(KIP_TEXT)) ? kip1->sections[KIP_TEXT].size_comp :
																  kip1->sections[KIP_TEXT].size_decomp;
				u32 size_comp   = kip1->sections[KIP_RODATA].size_comp;
				u32 size_decomp = kip1->sections[KIP_RODATA].size_decomp;
				if (kip1->flags & BIT(KIP_RODATA))
					blz_uncompress_srcdest(&kip1->data[offset], size_comp, ro_data, size_decomp);
				else
					memcpy(ro_data, &kip1->data[offset], size_decomp);

				for (u32 i = 0; i < 0x100; i+= sizeof(u32))
				{
					// Check size and name of nss matches.
					if (*(u32 *)&ro_data[i] == 8 && !memcmp("fs.exfat", &ro_data[i + 4], 8))
					{
						kip_name = "FS_exfat";
						break;
					}
				}

				free(ro_data);
			}

			s_printf(filename, "%s.kip1", kip_name);
			emmcsn_path_impl(path, pkg2ini_paths[idx], filename, &emmc_storage);
			if (sd_save_to_file(kip1, kip1_size, path))
				goto out;

			if (strcmp((const char*)kip1->name, "Loader") == 0)
			{
				s_printf(txt_buf + strlen(txt_buf), " %s             >  #00FFCC %s.kip1#\n", kip1->name, kip_name);
			}
			else if (strcmp((const char*)kip1->name, "sm") == 0 || strcmp((const char*)kip1->name, "FS") == 0)
			{
				s_printf(txt_buf + strlen(txt_buf), " %s                 >  #00FFCC %s.kip1#\n", kip1->name, kip_name);
			}
			else if (strcmp((const char*)kip1->name, "spl") == 0 || strcmp((const char*)kip1->name, "NCM") == 0 ||
					 strcmp((const char*)kip1->name, "PCV") == 0 || strcmp((const char*)kip1->name, "Bus") == 0 ||
					 strcmp((const char*)kip1->name, "psc") == 0)
			{
				s_printf(txt_buf + strlen(txt_buf), " %s                >  #00FFCC %s.kip1#\n", kip1->name, kip_name);
			}
			else if (strcmp((const char*)kip1->name, "boot") == 0)
			{
				s_printf(txt_buf + strlen(txt_buf), " %s               >  #00FFCC %s.kip1#\n", kip1->name, kip_name);
			}
			else
			{
				s_printf(txt_buf + strlen(txt_buf), " %s        >  #00FFCC %s.kip1#\n", kip1->name, kip_name);
			}

			lv_label_set_text(lb_log, txt_buf);
			manual_system_maintenance(true);

			ptr += kip1_size;
		}

		const char *newline_prefix = !idx ? "\n\n\n\n" : "\n";

		s_printf(txt_buf + strlen(txt_buf),
			"%s #C7EA46 [# #00FF00 sdmc:/backup/##00FFCC %s##00FF00 /pkg/##00DDFF %s##00FF00 /pkg1# #C7EA46 ]# 백 업  완 료.\n"
			" #C7EA46 [# #00FF00 sdmc:/backup/##00FFCC %s##00FF00 /pkg/##00DDFF %s##00FF00 /pkg2# #C7EA46 ]# 백 업  완 료.",
			newline_prefix, sn, !idx ? "Main" : "Safe", sn, !idx ? "Main" : "Safe"
		);
		lv_label_set_text(lb_log, txt_buf);
		manual_system_maintenance(true);
	}

out:
	emmc_gpt_free(&gpt);
out_free:
	free(pkg1);
	free(secmon);
	free(warmboot);
	free(loader);
	free(pkg2);
	free(txt_buf);
	emmc_end();
	sd_unmount();

	if (mkey >= HOS_MKEY_VER_620)
		se_aes_key_clear(8);
out_end:
	// Enable buttons.
	nyx_window_toggle_buttons(win, false);

	return LV_RES_OK;
}
