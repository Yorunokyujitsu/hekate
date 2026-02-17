/*
 * Copyright (c) 2019-2026 CTCaer
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
#include "fe_emummc_tools.h"
#include "gui_tools_partition_manager.h"
#include <libs/fatfs/ff.h>

//===================================================================
//  ASAP: Additional NAND-related includes and extern declarations.
//===================================================================
#include "gui_info.h"
#include "gui_options.h"
#include "gui_tools.h"
#include "gui_emmc_tools.h"
#include "../config.h"
extern nyx_config n_cfg;
//===================================================================

extern char *emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage);

typedef struct _mbr_ctxt_t
{
	u32 available;
	u32 sector[3];
	u32 resized_cnt[3];

	int part_idx;
	u32 sector_start;
} mbr_ctxt_t;

static bool emummc_backup;
static mbr_ctxt_t mbr_ctx;
static lv_obj_t *emummc_manage_window;
static lv_res_t (*emummc_tools)(lv_obj_t *btn);

static lv_res_t _action_emummc_window_close(lv_obj_t *btn)
{
	nyx_win_close_action_custom(btn);

	// Delete and relaunch main emuMMC window.
	lv_obj_del(emummc_manage_window);
	(*emummc_tools)(NULL);

	return LV_RES_INV;
}

static void _create_window_emummc()
{
	emmc_tool_gui_t emmc_tool_gui_ctxt;

	lv_obj_t *win;
	if (!mbr_ctx.part_idx)
		win = nyx_create_window_custom_close_btn(SYMBOL_DRIVE"  파일 에뮤낸드 생성", _action_emummc_window_close);
	else
		win = nyx_create_window_custom_close_btn(SYMBOL_DRIVE"  파티션 에뮤낸드 생성", _action_emummc_window_close);

	//Disable buttons.
	nyx_window_toggle_buttons(win, true);

	// Create important info container.
	lv_obj_t *h1 = lv_cont_create(win, NULL);
	lv_cont_set_fit(h1, false, true);
	lv_obj_set_width(h1, (LV_HOR_RES / 9) * 5);
	lv_obj_set_click(h1, false);
	lv_cont_set_layout(h1, LV_LAYOUT_OFF);

	static lv_style_t h_style;
	lv_style_copy(&h_style, lv_cont_get_style(h1));
	h_style.body.main_color = LV_COLOR_HEX(0x151524);
	h_style.body.grad_color = h_style.body.main_color;
	h_style.body.opa = LV_OPA_COVER;

	// Chreate log container.
	lv_obj_t *h2 = lv_cont_create(win, h1);
	lv_cont_set_style(h2, &h_style);
	lv_cont_set_fit(h2, false, false);
	lv_obj_set_size(h2, (LV_HOR_RES / 11) * 4, LV_DPI * 5);
	lv_cont_set_layout(h2, LV_LAYOUT_OFF);
	lv_obj_align(h2, h1, LV_ALIGN_OUT_RIGHT_TOP, 0, LV_DPI / 5);

	lv_obj_t *label_log = lv_label_create(h2, NULL);
	lv_label_set_recolor(label_log, true);
	lv_obj_set_style(label_log, &monospace_text);
	lv_label_set_long_mode(label_log, LV_LABEL_LONG_BREAK);
	lv_label_set_static_text(label_log, "");
	lv_obj_set_width(label_log, lv_obj_get_width(h2));
	lv_obj_align(label_log, h2, LV_ALIGN_IN_TOP_LEFT, LV_DPI / 10, LV_DPI / 10);
	emmc_tool_gui_ctxt.label_log = label_log;

	// Create elements for info container.
	lv_obj_t *label_sep = lv_label_create(h1, NULL);
	lv_label_set_static_text(label_sep, "");

	lv_obj_t *label_info = lv_label_create(h1, NULL);
	lv_label_set_recolor(label_info, true);
	lv_obj_set_width(label_info, lv_obj_get_width(h1));
	lv_label_set_static_text(label_info, "\n\n\n\n\n\n\n\n\n");
	lv_obj_set_style(label_info, lv_theme_get_current()->label.prim);
	lv_obj_align(label_info, label_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, LV_DPI / 10);
	emmc_tool_gui_ctxt.label_info = label_info;

	lv_obj_t *bar = lv_bar_create(h1, NULL);
	lv_obj_set_size(bar, LV_DPI * 38 / 10, LV_DPI / 5);
	lv_bar_set_range(bar, 0, 100);
	lv_bar_set_value(bar, 0);
	lv_obj_align(bar, label_info, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 8);
	lv_obj_set_opa_scale(bar, LV_OPA_0);
	lv_obj_set_opa_scale_enable(bar, true);
	emmc_tool_gui_ctxt.bar = bar;

	lv_obj_t *label_pct= lv_label_create(h1, NULL);
	lv_label_set_recolor(label_pct, true);
	lv_label_set_static_text(label_pct, " "SYMBOL_DOT" 0%");
	lv_obj_set_style(label_pct, lv_theme_get_current()->label.prim);
	lv_obj_align(label_pct, bar, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 20, 0);
	lv_obj_set_opa_scale(label_pct, LV_OPA_0);
	lv_obj_set_opa_scale_enable(label_pct, true);
	emmc_tool_gui_ctxt.label_pct = label_pct;

	lv_obj_t *label_finish = lv_label_create(h1, NULL);
	lv_label_set_recolor(label_finish, true);
	lv_label_set_static_text(label_finish, "");
	lv_obj_set_style(label_finish, lv_theme_get_current()->label.prim);
	lv_obj_align(label_finish, bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI * 9 / 20);
	emmc_tool_gui_ctxt.label_finish = label_finish;

	if (!mbr_ctx.part_idx)
		dump_emummc_file(&emmc_tool_gui_ctxt);
	else
		dump_emummc_raw(&emmc_tool_gui_ctxt, mbr_ctx.part_idx, mbr_ctx.sector_start, mbr_ctx.resized_cnt[mbr_ctx.part_idx - 1]);

	nyx_window_toggle_buttons(win, false);
}

static lv_res_t _create_emummc_raw_format(lv_obj_t * btns, const char * txt)
{
	int btn_idx = lv_btnm_get_pressed(btns);

	// Delete parent mbox.
	mbox_action(btns, txt);

	// Create partition window.
	if (!btn_idx)
		create_window_sd_partition_manager(btns);

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	return LV_RES_INV;
}

static lv_res_t _create_emummc_raw_action(lv_obj_t * btns, const char * txt)
{
	int btn_idx = lv_btnm_get_pressed(btns);
	lv_obj_t *bg = lv_obj_get_parent(lv_obj_get_parent(btns));

	mbr_ctx.sector_start = 0x8000; // Protective offset.

	switch (btn_idx)
	{
	case 0:
		mbr_ctx.part_idx = 1;
		mbr_ctx.sector_start += mbr_ctx.sector[0];
		break;
	case 1:
		mbr_ctx.part_idx = 2;
		mbr_ctx.sector_start += mbr_ctx.sector[1];
		break;
	case 2:
		mbr_ctx.part_idx = 3;
		mbr_ctx.sector_start += mbr_ctx.sector[2];
		break;
	default:
		break;
	}

	if (btn_idx < 3)
	{
		lv_obj_set_style(bg, &lv_style_transp);
		_create_window_emummc();
	}

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	mbox_action(btns, txt);

	return LV_RES_INV;
}

static void _create_mbox_emummc_raw()
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_format[] = { "\222확인", "\222취소", "" };
	static char *mbox_btn_parts[] = { "\262파티션 1", "\262파티션 2", "\262파티션 3", "\222취소", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 6);

	char *txt_buf = (char *)malloc(SZ_16K);
	mbr_t *mbr = (mbr_t *)malloc(sizeof(mbr_t));

	memset(&mbr_ctx, 0, sizeof(mbr_ctxt_t));

	sd_mount();
	sdmmc_storage_read(&sd_storage, 0, 1, mbr);
	sd_unmount();

	emmc_initialize(false);

	u32 emmc_size_safe = emmc_storage.sec_cnt + 0xC000; // eMMC GPP size + BOOT0/1.

	emmc_end();

	for (int i = 1; i < 4; i++)
	{
		u32 part_size = mbr->partitions[i].size_sct;
		u32 part_start = mbr->partitions[i].start_sct;
		u8  part_type = mbr->partitions[i].type;

		// Skip Linux, GPT (Android) and SFD partitions.
		bool valid_part = (part_type != 0x83) && (part_type != 0xEE) && (part_type != 0xFF);

		// Check if at least 4GB and start above 16MB.
		if ((part_size >= 0x80F000) && part_start > 0x8000 && valid_part)
		{
			mbr_ctx.available |= BIT(i - 1);
			mbr_ctx.sector[i - 1] = part_start;

			// Only allow up to 28GB resized emuMMC.
			if (part_size <= 0x3810000)
				mbr_ctx.resized_cnt[i - 1] = part_size - 0xC000; // Save sectors count without protective size and BOOT0/1.
			else if (part_size >= emmc_size_safe)
				mbr_ctx.resized_cnt[i - 1] = 0;
			else
			{
				mbr_ctx.available &= ~BIT(i - 1);
				mbr_ctx.sector[i - 1] = 0;
			}
		}
	}

	if (mbr_ctx.available)
	{
		s_printf(txt_buf,
			"#008EED 파티션 매니저#\n\n"
			"#FFBA00 안내#: 생성하려는 #C7EA46 파티션 번호#를 선택하세요.\n");

		if (mbr_ctx.resized_cnt[0] || mbr_ctx.resized_cnt[1] || mbr_ctx.resized_cnt[2]) {
			strcat(txt_buf, "사용 가능한 파티션이 활성화됩니다.\n\n");
		}
	}
	else
		s_printf(txt_buf,
			"#008EED 파티션 매니저#\n\n"
			"#FFBA00 안내#: 생성 가능한 파티션이 없습니다!\n"
			"SD 카드를 파티션 분할하시겠습니까?\n\n");

	s_printf(txt_buf + strlen(txt_buf),
		"- #C7EA46 파티션 테이블# -\n"
		"#C0C0C0 파티션 0: 유형: %02x, 섹터: %08x, 크기: %08x#\n"
		"#%s 파티션 1: 유형: %02x, 섹터: %08x, 크기: %08x#\n"
		"#%s 파티션 2: 유형: %02x, 섹터: %08x, 크기: %08x#\n"
		"#%s 파티션 3: 유형: %02x, 섹터: %08x, 크기: %08x#",
		mbr->partitions[0].type, mbr->partitions[0].start_sct, mbr->partitions[0].size_sct,
		(mbr_ctx.available & BIT(0)) ? (mbr_ctx.resized_cnt[0] ? "FFBA00" : "C7EA46") : "C0C0C0",
		 mbr->partitions[1].type, mbr->partitions[1].start_sct, mbr->partitions[1].size_sct,
		(mbr_ctx.available & BIT(1)) ? (mbr_ctx.resized_cnt[1] ? "FFBA00" : "C7EA46") : "C0C0C0",
		 mbr->partitions[2].type, mbr->partitions[2].start_sct, mbr->partitions[2].size_sct,
		(mbr_ctx.available & BIT(2)) ? (mbr_ctx.resized_cnt[2] ? "FFBA00" : "C7EA46") : "C0C0C0",
		 mbr->partitions[3].type, mbr->partitions[3].start_sct, mbr->partitions[3].size_sct);

	lv_mbox_set_text(mbox, txt_buf);
	free(txt_buf);
	free(mbr);

	if (mbr_ctx.available)
	{
		// Check available partitions and enable the corresponding buttons.
		if (mbr_ctx.available & 1)
			mbox_btn_parts[0][0] = '\222';
		else
			mbox_btn_parts[0][0] = '\262';
		if (mbr_ctx.available & 2)
			mbox_btn_parts[1][0] = '\222';
		else
			mbox_btn_parts[1][0] = '\262';
		if (mbr_ctx.available & 4)
			mbox_btn_parts[2][0] = '\222';
		else
			mbox_btn_parts[2][0] = '\262';

		lv_mbox_add_btns(mbox, (const char **)mbox_btn_parts, _create_emummc_raw_action);
	}
	else
		lv_mbox_add_btns(mbox, mbox_btn_format, _create_emummc_raw_format);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);
}

static lv_res_t _create_emummc_action(lv_obj_t * btns, const char * txt)
{
	int btn_idx = lv_btnm_get_pressed(btns);
	lv_obj_t *bg = lv_obj_get_parent(lv_obj_get_parent(btns));

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	switch (btn_idx)
	{
	case 0:
		lv_obj_set_style(bg, &lv_style_transp);
		_create_window_emummc();
		break;
	case 1:
		_create_mbox_emummc_raw();
		break;
	}

	mbox_action(btns, txt);

	return LV_RES_INV;
}

static lv_res_t _create_mbox_emummc_create(lv_obj_t *btn)
{
	if (!nyx_emmc_check_battery_enough())
		return LV_RES_OK;

	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char * mbox_btn_map[] = { "\222파일", "\222파티션", "\222취소", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 6);

	lv_mbox_set_text(mbox,
		"#008EED 낸드 매니저#\n\n"
		"#FFBA00 안내#: 생성하려는 낸드 #C7EA46 유형#을 선택하세요.\n\n"
		"#FF8000 주의:#\n#FF8000 유형 상관 없이, FAT32 시스템을 권장합니다.#\n"
		"#FF8000 exFAT은 백업 테이블이 없으므로, 유저 환경에 따라,#\n"
		"#FF8000 시스템 파일이 손상될 수 있습니다.#");

	lv_mbox_add_btns(mbox, mbox_btn_map, _create_emummc_action);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}

static void _change_raw_emummc_part_type()
{
	mbr_t *mbr = (mbr_t *)malloc(sizeof(mbr_t));
	sdmmc_storage_read(&sd_storage, 0, 1, mbr);
	mbr->partitions[mbr_ctx.part_idx].type = 0xE0;
	sdmmc_storage_write(&sd_storage, 0, 1, mbr);
	free(mbr);
}

static lv_res_t _save_emummc_cfg_mig_mbox_action(lv_obj_t *btns, const char *txt)
{
	// Delete main emuMMC and popup windows and relaunch main emuMMC window.
	lv_obj_del(emummc_manage_window);
	mbox_action(btns, txt);

	(*emummc_tools)(NULL);

	return LV_RES_INV;
}

static void _create_emummc_migrated_mbox()
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map[] = { "\251", "확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 4);

	lv_mbox_set_text(mbox,
		"#008EED 낸드 매니저#\n\n"
		"선택한 구성이 #96FF00 기본 낸드#로 저장되었습니다.");

	lv_mbox_add_btns(mbox, mbox_btn_map, _save_emummc_cfg_mig_mbox_action);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);
}

static void _migrate_sd_raw_based()
{
	mbr_ctx.sector_start = 2;

	sd_mount();
	f_mkdir("emuMMC");
	f_mkdir("emuMMC/ER00");

	f_rename("Emutendo", "emuMMC/ER00/Nintendo");
	FIL fp;
	f_open(&fp, "emuMMC/ER00/raw_based", FA_CREATE_ALWAYS | FA_WRITE);
	f_write(&fp, &mbr_ctx.sector_start, 4, NULL);
	f_close(&fp);

	save_emummc_cfg(1, mbr_ctx.sector_start, "emuMMC/ER00");
	_create_emummc_migrated_mbox();
	sd_unmount();
}

static void _migrate_sd_raw_emummc_based()
{
	char *tmp = (char *)malloc(0x80);
	s_printf(tmp, "emuMMC/RAW%d", mbr_ctx.part_idx);

	sd_mount();
	f_mkdir("emuMMC");
	f_mkdir(tmp);
	strcat(tmp, "/raw_based");

	FIL fp;
	if (!f_open(&fp, tmp, FA_CREATE_ALWAYS | FA_WRITE))
	{
		f_write(&fp, &mbr_ctx.sector_start, 4, NULL);
		f_close(&fp);
	}

	s_printf(tmp, "emuMMC/RAW%d", mbr_ctx.part_idx);

	_change_raw_emummc_part_type();

	save_emummc_cfg(mbr_ctx.part_idx, mbr_ctx.sector_start, tmp);
	_create_emummc_migrated_mbox();
	free(tmp);

	sd_unmount();
}

static void _migrate_sd_file_based()
{
	sd_mount();
	f_mkdir("emuMMC");
	f_mkdir("emuMMC/EF00");

	f_rename("Emutendo", "emuMMC/EF00/Nintendo");
	FIL fp;
	f_open(&fp, "emuMMC/EF00/file_based", FA_CREATE_ALWAYS | FA_WRITE);
	f_close(&fp);

	char *path = (char *)malloc(128);
	char *path2 = (char *)malloc(128);
	s_printf(path, "%c%c%c%c%s", 's', 'x', 'o', 's', "/emunand");
	f_rename(path, "emuMMC/EF00/eMMC");

	for (int i = 0; i < 2; i++)
	{
		s_printf(path, "emuMMC/EF00/eMMC/boot%d.bin", i);
		s_printf(path2, "emuMMC/EF00/eMMC/BOOT%d", i);
		f_rename(path, path2);
	}
	for (int i = 0; i < 8; i++)
	{
		s_printf(path, "emuMMC/EF00/eMMC/full.%02d.bin", i);
		s_printf(path2, "emuMMC/EF00/eMMC/%02d", i);
		f_rename(path, path2);
	}

	free(path);
	free(path2);

	save_emummc_cfg(0, 0, "emuMMC/EF00");
	_create_emummc_migrated_mbox();
	sd_unmount();
}

static void _migrate_sd_backup_file_based()
{
	char *emu_path = (char *)malloc(128);
	char *parts_path = (char *)malloc(128);
	char *backup_path = (char *)malloc(128);
	char *backup_file_path = (char *)malloc(128);

	sd_mount();
	f_mkdir("emuMMC");

	strcpy(emu_path, "emuMMC/BK");
	u32 base_len = strlen(emu_path);

	for (int j = 0; j < 100; j++)
	{
		update_emummc_base_folder(emu_path, base_len, j);
		if (f_stat(emu_path, NULL) == FR_NO_FILE)
			break;
	}
	base_len = strlen(emu_path);

	f_mkdir(emu_path);
	strcat(emu_path, "/eMMC");
	f_mkdir(emu_path);

	FIL fp;
	// Create file based flag.
	strcpy(emu_path + base_len, "/file_based");
	f_open(&fp, "emuMMC/BK00/file_based", FA_CREATE_ALWAYS | FA_WRITE);
	f_close(&fp);

	if (!emummc_backup)
		emmcsn_path_impl(backup_path, "", "", NULL);
	else
		emmcsn_path_impl(backup_path, "/emummc", "", NULL);

	// Move BOOT0.
	s_printf(backup_file_path, "%s/BOOT0", backup_path);
	strcpy(emu_path + base_len, "/eMMC/BOOT0");
	f_rename(backup_file_path, emu_path);

	// Move BOOT1.
	s_printf(backup_file_path, "%s/BOOT1", backup_path);
	strcpy(emu_path + base_len, "/eMMC/BOOT1");
	f_rename(backup_file_path, emu_path);

	// Move raw GPP.
	bool multipart = false;
	s_printf(backup_file_path, "%s/rawnand.bin", backup_path);

	if (f_stat(backup_file_path, NULL))
		multipart = true;

	if (!multipart)
	{
		strcpy(emu_path + base_len, "/eMMC/00");
		f_rename(backup_file_path, emu_path);
	}
	else
	{
		emu_path[base_len] = 0;
		for (int i = 0; i < 32; i++)
		{
			s_printf(backup_file_path, "%s/rawnand.bin.%02d", backup_path, i);
			s_printf(parts_path, "%s/eMMC/%02d", emu_path, i);
			if (f_rename(backup_file_path, parts_path))
				break;
		}
	}

	free(emu_path);
	free(parts_path);
	free(backup_path);
	free(backup_file_path);

	save_emummc_cfg(0, 0, "emuMMC/BK00");
	_create_emummc_migrated_mbox();
	sd_unmount();
}

static lv_res_t _create_emummc_mig1_action(lv_obj_t * btns, const char * txt)
{
	switch (lv_btnm_get_pressed(btns))
	{
	case 0:
		_migrate_sd_file_based();
		break;
	case 1:
		_migrate_sd_raw_based();
		break;
	}

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	mbox_action(btns, txt);

	return LV_RES_INV;
}

static lv_res_t _create_emummc_mig0_action(lv_obj_t * btns, const char * txt)
{
	switch (lv_btnm_get_pressed(btns))
	{
	case 0:
		_migrate_sd_file_based();
		break;
	}

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	mbox_action(btns, txt);

	return LV_RES_INV;
}

static lv_res_t _create_emummc_mig2_action(lv_obj_t * btns, const char * txt)
{
	switch (lv_btnm_get_pressed(btns))
	{
	case 0:
		_migrate_sd_raw_based();
		break;
	}

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	mbox_action(btns, txt);

	return LV_RES_INV;
}

static lv_res_t _create_emummc_mig3_action(lv_obj_t * btns, const char * txt)
{
	switch (lv_btnm_get_pressed(btns))
	{
	case 0:
		_migrate_sd_raw_emummc_based();
		break;
	}

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	mbox_action(btns, txt);

	return LV_RES_INV;
}

static lv_res_t _create_emummc_mig4_action(lv_obj_t * btns, const char * txt)
{
	switch (lv_btnm_get_pressed(btns))
	{
	case 0:
		_migrate_sd_backup_file_based();
		break;
	}

	mbr_ctx.part_idx = 0;
	mbr_ctx.sector_start = 0;

	mbox_action(btns, txt);

	return LV_RES_INV;
}

bool em_raw;
bool em_file;
static lv_res_t _create_emummc_migrate_action(lv_obj_t * btns, const char * txt)
{
	bool backup = false;
	bool emummc = false;

	switch (lv_btnm_get_pressed(btns))
	{
	case 0:
		backup = true;
		break;
	case 1:
		emummc = true;
		break;
	case 2:
		break;
	case 3:
		mbox_action(btns, txt);
		return LV_RES_INV;
	}

	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map[] = { "\222예", "\222아니오", "" };
	static const char *mbox_btn_map1[] = { "\222파일", "\222파티션", "\222취소", "" };
	static const char *mbox_btn_map3[] = { "\251", "확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 6);

	char *txt_buf = (char *)malloc(SZ_16K);

	if (backup)
	{
		if (!emummc_backup)
			s_printf(txt_buf,
				"#008EED 낸드 매니저#\n\n"
				"백업된 #C7EA46 파티션# 낸드를 찾았습니다!\n"
				"#FF8000 파일# 에뮤낸드로 이사하시겠습니까?\n");
		else
			s_printf(txt_buf,
				"#008EED 낸드 매니저#\n\n"
				"백업된 #C7EA46 파티션# 에뮤낸드를 찾았습니다!\n"
				"#FF8000 파일# 에뮤낸드로 이사하시겠습니까?\n");
		lv_mbox_add_btns(mbox, mbox_btn_map, _create_emummc_mig4_action);
	}
	else if (emummc)
	{
		s_printf(txt_buf,
			"#008EED 낸드 매니저#\n\n"
			"#C7EA46 파티션# 에뮤낸드를 찾았습니다!\n"
			"올바른 #FF8000 섹터#로 수정하시겠습니까?\n");
		lv_mbox_add_btns(mbox, mbox_btn_map, _create_emummc_mig3_action);
	}
	else if (em_raw && em_file)
	{
		s_printf(txt_buf,
			"#008EED 낸드 매니저#\n\n"
			"#C7EA46 SXOS# 파티션 및 파일 에뮤낸드를 찾았습니다!\n"
			"#FF8000 Hekate# 기반으로 이사할 유형 선택:\n");
		lv_mbox_add_btns(mbox, mbox_btn_map1, _create_emummc_mig1_action);
	}
	else if (em_raw)
	{
		s_printf(txt_buf,
			"#008EED 낸드 매니저#\n\n"
			"#C7EA46 SXOS# 파티션 에뮤낸드를 찾았습니다!\n"
			"#FF8000 Hekate# 기반으로 이사하시겠습니까?\n");
		lv_mbox_add_btns(mbox, mbox_btn_map, _create_emummc_mig2_action);
	}
	else if (em_file)
	{
		s_printf(txt_buf,
			"#008EED 낸드 매니저#\n\n"
			"#C7EA46 SXOS# 파일 에뮤낸드를 찾았습니다!\n"
			"#FF8000 Hekate# 기반으로 이사하시겠습니까?\n");
		lv_mbox_add_btns(mbox, mbox_btn_map, _create_emummc_mig0_action);
	}
	else
	{
		s_printf(txt_buf, "#008EED 낸드 매니저#\n에뮤낸드를 찾을 수 없습니다!\n");
		lv_mbox_add_btns(mbox, mbox_btn_map3, mbox_action);
	}

	lv_mbox_set_text(mbox, txt_buf);
	free(txt_buf);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	mbox_action(btns, txt);

	return LV_RES_INV;
}

typedef struct _emummc_images_t
{
	dirlist_t *dirlist;
	u32 part_sector[3];
	u32 part_type[3];
	u32 part_end[3];
	char part_path[3 * 128];
	lv_obj_t *win;
} emummc_images_t;

static lv_res_t _create_mbox_emummc_migrate(lv_obj_t *btn)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static char *mbox_btn_map[] = { "\262파일", "\262파티션", "\262SXOS", "\222취소", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 6);

	lv_mbox_set_text(mbox,
		"#008EED 마이그레이션 매니저#\n\n"
		"#00DDFF 파일#: 백업한 #C7EA46 파티션# 에뮤낸드를 #C7EA46 파일# 에뮤낸드로 이사합니다.   \n"
		"#00DDFF 파티션#: 복원한 #C7EA46 파티션# 에뮤낸드의 #C7EA46 섹터#를 수정합니다.          \n"
		"#00DDFF SXOS#: #FF8000 SX 낸드#, #FF8000 Emutendo#를 #C7EA46 Hekate# 기반으로 이사합니다.");

	char *path_buf = (char *)malloc(0x512);
	mbr_t *mbr = (mbr_t *)malloc(sizeof(mbr_t));
	u8 *efi_part = (u8 *)malloc(0x200);

	sd_mount();
	sdmmc_storage_read(&sd_storage, 0, 1, mbr);

	emmc_initialize(false);

	em_raw = false;
	em_file = false;
	bool backup = false;
	bool emummc = false;
	bool rawnand_backup = false;

	mbr_ctx.sector_start = 0;
	mbr_ctx.part_idx = 0;

	// Try to find a partition based emuMMC.
	for (int i = 1; i < 4; i++)
	{
		mbr_ctx.sector_start = mbr->partitions[i].start_sct;

		if (!mbr_ctx.sector_start)
			continue;

		sdmmc_storage_read(&sd_storage, mbr_ctx.sector_start + 0xC001, 1, efi_part);
		if (!memcmp(efi_part, "EFI PART", 8))
		{
			mbr_ctx.sector_start += 0x8000;
			emummc = true;
			mbr_ctx.part_idx = i;
			break;
		}
		else
		{
			sdmmc_storage_read(&sd_storage, mbr_ctx.sector_start + 0x4001, 1, efi_part);
			if (!memcmp(efi_part, "EFI PART", 8))
			{
				emummc = true;
				mbr_ctx.part_idx = i;
				break;
			}
		}
	}

	if (!mbr_ctx.part_idx)
	{
		sdmmc_storage_read(&sd_storage, 0x4003, 1, efi_part);
		if (!memcmp(efi_part, "EFI PART", 8))
			em_raw = true;
	}

	s_printf(path_buf, "%c%c%c%c%s", 's', 'x', 'o','s', "/emunand/boot0.bin");

	if (!f_stat(path_buf, NULL))
		em_file = true;

	emummc_backup = false;

	emmcsn_path_impl(path_buf, "", "BOOT0", &emmc_storage);
	if (!f_stat(path_buf, NULL))
		backup = true;

	emmcsn_path_impl(path_buf, "", "rawnand.bin", &emmc_storage);
	if (!f_stat(path_buf, NULL))
		rawnand_backup = true;

	emmcsn_path_impl(path_buf, "", "rawnand.bin.00", &emmc_storage);
	if (!f_stat(path_buf, NULL))
		rawnand_backup = true;

	backup = backup && rawnand_backup;

	if (!backup)
	{
		rawnand_backup = false;
		emummc_backup = true;

		emmcsn_path_impl(path_buf, "/emummc", "BOOT0", &emmc_storage);
		if (!f_stat(path_buf, NULL))
			backup = true;

		emmcsn_path_impl(path_buf, "/emummc", "rawnand.bin", &emmc_storage);
		if (!f_stat(path_buf, NULL))
			rawnand_backup = true;

		emmcsn_path_impl(path_buf, "/emummc", "rawnand.bin.00", &emmc_storage);
		if (!f_stat(path_buf, NULL))
			rawnand_backup = true;

		backup = backup && rawnand_backup;
	}

	sd_unmount();
	emmc_end();

	// Check available types and enable the corresponding buttons.
	if (backup)
		mbox_btn_map[0][0] = '\222';
	else
		mbox_btn_map[0][0] = '\262';
	if (emummc)
		mbox_btn_map[1][0] = '\222';
	else
		mbox_btn_map[1][0] = '\262';
	if (em_raw || em_file)
		mbox_btn_map[2][0] = '\222';
	else
		mbox_btn_map[2][0] = '\262';

	free(path_buf);
	free(mbr);
	free(efi_part);

	lv_mbox_add_btns(mbox, (const char **)mbox_btn_map, _create_emummc_migrate_action);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}

static emummc_images_t *emummc_img;

static lv_res_t _save_emummc_cfg_mbox_action(lv_obj_t *btns, const char *txt)
{
	// Free components, delete main emuMMC and popup windows and relaunch main emuMMC window.
	lv_obj_del(emummc_img->win);
	lv_obj_del(emummc_manage_window);
	free(emummc_img->dirlist);
	free(emummc_img);

	mbox_action(btns, txt);

	(*emummc_tools)(NULL);

	return LV_RES_INV;
}

static void _create_emummc_saved_mbox()
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map[] = { "\251", "확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 4);

	lv_mbox_set_text(mbox,
		"#008EED 낸드 매니저#\n\n"
		"선택한 구성이 #96FF00 기본 낸드#로 저장되었습니다.");

	lv_mbox_add_btns(mbox, mbox_btn_map, _save_emummc_cfg_mbox_action);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);
}

static lv_res_t _save_raw_emummc_cfg_action(lv_obj_t * btn)
{
	lv_btn_ext_t *ext = lv_obj_get_ext_attr(btn);
	switch (ext->idx)
	{
	case 0:
		save_emummc_cfg(1, emummc_img->part_sector[0], &emummc_img->part_path[0]);
		break;
	case 1:
		save_emummc_cfg(2, emummc_img->part_sector[1], &emummc_img->part_path[128]);
		break;
	case 2:
		save_emummc_cfg(3, emummc_img->part_sector[2], &emummc_img->part_path[256]);
		break;
	}

	_create_emummc_saved_mbox();
	sd_unmount();

	return LV_RES_INV;
}

//=====================================================
//  ASAP: NAND change option - emuMMC enabled toggle.
//=====================================================
static lv_res_t _save_disable_emummc_cfg_action(lv_obj_t *btn)
{
	FIL rfp, wfp;
	FRESULT res;
	UINT br, bw;
	char *buf, *newbuf, *p, *line_end;
	const size_t BUF_SIZE = 16 * 1024;

	sd_mount();

	res = f_open(&rfp, "emuMMC/emummc.ini", FA_READ);
	if (res != FR_OK) {
		sd_unmount();
		return LV_RES_OK;
	}

	buf = malloc(BUF_SIZE);
	f_read(&rfp, buf, BUF_SIZE - 1, &br);
	buf[br] = '\0';
	f_close(&rfp);

	p = strstr(buf, "[emummc]");
	if (p && (p = strstr(p, "enabled="))) {
		p += strlen("enabled=");
		int curr = atoi(p);
		
		if (curr == 1) {
			line_end = p;
			while (*line_end && *line_end != '\r' && *line_end != '\n') {
				line_end++;
			}

			newbuf = malloc(BUF_SIZE);
			size_t prefix = p - buf;
			memcpy(newbuf, buf, prefix);
			s_printf(newbuf + prefix, "%d", 0);
			strcpy(newbuf + prefix + strlen(newbuf + prefix), line_end);

			res = f_open(&wfp, "emuMMC/emummc.ini", FA_WRITE | FA_CREATE_ALWAYS);
			if (res == FR_OK) {
				f_write(&wfp, newbuf, strlen(newbuf), &bw);
				f_close(&wfp);
			}
			free(newbuf);
		}
	}
	_create_emummc_saved_mbox();

	free(buf);
	sd_unmount();
	return LV_RES_OK;
}
//=====================================================

static lv_res_t _save_file_emummc_cfg_action(lv_obj_t *btn)
{
	save_emummc_cfg(0, 0, lv_list_get_btn_text(btn));
	_create_emummc_saved_mbox();
	sd_unmount();

	return LV_RES_INV;
}

static lv_res_t _action_win_change_emummc_close(lv_obj_t *btn)
{
	free(emummc_img->dirlist);
	free(emummc_img);

	return nyx_win_close_action_custom(btn);
}

static lv_res_t _create_change_emummc_window(lv_obj_t *btn_caller)
{
	lv_obj_t *win = nyx_create_window_custom_close_btn(SYMBOL_SETTINGS"  낸드 변경", _action_win_change_emummc_close);
	lv_win_add_btn(win, NULL, SYMBOL_POWER" 시스낸드", _save_disable_emummc_cfg_action);

	sd_mount();

	emummc_img = malloc(sizeof(emummc_images_t));
	emummc_img->win = win;

	mbr_t *mbr = (mbr_t *)malloc(sizeof(mbr_t));
	char *path = malloc(512);

	sdmmc_storage_read(&sd_storage, 0, 1, mbr);

	memset(emummc_img->part_path, 0, 3 * 128);

	for (int i = 1; i < 4; i++)
	{
		emummc_img->part_sector[i - 1] = mbr->partitions[i].start_sct;
		emummc_img->part_end[i - 1]    = emummc_img->part_sector[i - 1] + mbr->partitions[i].size_sct - 1;
		emummc_img->part_type[i - 1]   = mbr->partitions[i].type;
	}
	free(mbr);

	emummc_img->dirlist = dirlist("emuMMC", NULL, DIR_SHOW_DIRS);

	if (!emummc_img->dirlist)
		goto out0;

	u32 emummc_idx = 0;

	FIL fp;

	// Check for sd raw partitions, based on the folders in /emuMMC.
	while (emummc_img->dirlist->name[emummc_idx])
	{
		s_printf(path, "emuMMC/%s/raw_based", emummc_img->dirlist->name[emummc_idx]);

		if (!f_stat(path, NULL))
		{
			f_open(&fp, path, FA_READ);
			u32 curr_list_sector = 0;
			f_read(&fp, &curr_list_sector, 4, NULL);
			f_close(&fp);

			// Check if there's a HOS image there.
			if ((curr_list_sector == 2) || (emummc_img->part_sector[0] && curr_list_sector >= emummc_img->part_sector[0] &&
				curr_list_sector < emummc_img->part_end[0] && emummc_img->part_type[0] != 0x83))
			{
				s_printf(&emummc_img->part_path[0], "emuMMC/%s", emummc_img->dirlist->name[emummc_idx]);
				emummc_img->part_sector[0] = curr_list_sector;
				emummc_img->part_end[0]    = 0;
			}
			else if (emummc_img->part_sector[1] && curr_list_sector >= emummc_img->part_sector[1] &&
				curr_list_sector < emummc_img->part_end[1] && emummc_img->part_type[1] != 0x83)
			{
				s_printf(&emummc_img->part_path[1 * 128], "emuMMC/%s", emummc_img->dirlist->name[emummc_idx]);
				emummc_img->part_sector[1] = curr_list_sector;
				emummc_img->part_end[1]    = 0;
			}
			else if (emummc_img->part_sector[2] && curr_list_sector >= emummc_img->part_sector[2] &&
				curr_list_sector < emummc_img->part_end[2] && emummc_img->part_type[2] != 0x83)
			{
				s_printf(&emummc_img->part_path[2 * 128], "emuMMC/%s", emummc_img->dirlist->name[emummc_idx]);
				emummc_img->part_sector[2] = curr_list_sector;
				emummc_img->part_end[2]    = 0;
			}
		}
		emummc_idx++;
	}

	emummc_idx = 0;
	u32 file_based_idx = 0;

	// Sanitize the directory list with sd file based ones.
	while (emummc_img->dirlist->name[emummc_idx])
	{
		s_printf(path, "emuMMC/%s/file_based", emummc_img->dirlist->name[emummc_idx]);

		if (!f_stat(path, NULL))
		{
			char *tmp = emummc_img->dirlist->name[emummc_idx];
			memcpy(emummc_img->dirlist->name[file_based_idx], tmp, strlen(tmp) + 1);
			file_based_idx++;
		}
		emummc_idx++;
	}
	emummc_img->dirlist->name[file_based_idx] = NULL;

out0:;
	static lv_style_t h_style;
	lv_style_copy(&h_style, &lv_style_transp);
	h_style.body.padding.inner = 0;
	h_style.body.padding.hor = LV_DPI - (LV_DPI / 4);
	h_style.body.padding.ver = LV_DPI / 6;

	// Create SD Raw Partitions container.
	lv_obj_t *h1 = lv_cont_create(win, NULL);
	lv_cont_set_style(h1, &h_style);
	lv_cont_set_fit(h1, false, true);
	lv_obj_set_width(h1, (LV_HOR_RES / 9) * 4);
	lv_obj_set_click(h1, false);
	lv_cont_set_layout(h1, LV_LAYOUT_OFF);

	lv_obj_t *label_sep = lv_label_create(h1, NULL);
	lv_label_set_static_text(label_sep, "");

	lv_obj_t *label_txt = lv_label_create(h1, NULL);
	lv_label_set_static_text(label_txt, "파티션 에뮤낸드");
	lv_obj_set_style(label_txt, lv_theme_get_current()->label.prim);
	lv_obj_align(label_txt, label_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, -(LV_DPI / 2));

	lv_obj_t *line_sep = lv_line_create(h1, NULL);
	static const lv_point_t line_pp[] = { {0, 0}, { LV_HOR_RES - (LV_DPI - (LV_DPI / 4)) * 2, 0} };
	lv_line_set_points(line_sep, line_pp, 2);
	lv_line_set_style(line_sep, lv_theme_get_current()->line.decor);
	lv_obj_align(line_sep, label_txt, LV_ALIGN_OUT_BOTTOM_LEFT, -(LV_DPI / 4), LV_DPI / 8);

	lv_obj_t *btn = NULL;
	lv_btn_ext_t *ext;
	lv_obj_t *btn_label = NULL;
	lv_obj_t *lv_desc = NULL;
	char *txt_buf = malloc(SZ_16K);

	// Create RAW buttons.
	for (u32 raw_btn_idx = 0; raw_btn_idx < 3; raw_btn_idx++)
	{
		btn = lv_btn_create(h1, btn);
		ext = lv_obj_get_ext_attr(btn);
		ext->idx = raw_btn_idx;
		btn_label = lv_label_create(btn, btn_label);

		lv_btn_set_state(btn, LV_BTN_STATE_REL);
		lv_obj_set_click(btn, true);

		if (emummc_img->part_type[raw_btn_idx] != 0x83)
		{
			s_printf(txt_buf, "SD RAW %d", raw_btn_idx + 1);
			lv_label_set_text(btn_label, txt_buf);
		}

		if (!emummc_img->part_sector[raw_btn_idx] || emummc_img->part_type[raw_btn_idx] == 0x83 || !emummc_img->part_path[raw_btn_idx * 128])
		{
			lv_btn_set_state(btn, LV_BTN_STATE_INA);
			lv_obj_set_click(btn, false);

			if (emummc_img->part_type[raw_btn_idx] == 0x83)
				lv_label_set_static_text(btn_label, "Linux");
		}

		if (!raw_btn_idx)
		{
			lv_btn_set_fit(btn, false, true);
			lv_obj_set_width(btn, LV_DPI * 3);
			lv_obj_align(btn, line_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 2, LV_DPI / 5);
		}
		else
			lv_obj_align(btn, lv_desc, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 3);

		lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, _save_raw_emummc_cfg_action);

		lv_desc = lv_label_create(h1, lv_desc);
		lv_label_set_recolor(lv_desc, true);
		lv_obj_set_style(lv_desc, &hint_small_style);

		s_printf(txt_buf, "섹터: 0x%08X\n폴더: %s", emummc_img->part_sector[raw_btn_idx], &emummc_img->part_path[raw_btn_idx * 128]);
		lv_label_set_text(lv_desc, txt_buf);
		lv_obj_align(lv_desc, btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 5);
	}
	free(txt_buf);

	// Create SD File Based container.
	lv_obj_t *h2 = lv_cont_create(win, NULL);
	lv_cont_set_style(h2, &h_style);
	lv_cont_set_fit(h2, false, true);
	lv_obj_set_width(h2, (LV_HOR_RES / 9) * 4);
	lv_obj_set_click(h2, false);
	lv_cont_set_layout(h2, LV_LAYOUT_OFF);
	lv_obj_align(h2, h1, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI * 17 / 29, 0);

	label_sep = lv_label_create(h2, NULL);
	lv_label_set_static_text(label_sep, "");

	lv_obj_t *label_txt3 = lv_label_create(h2, NULL);
	lv_label_set_static_text(label_txt3, "파일 에뮤낸드");
	lv_obj_set_style(label_txt3, lv_theme_get_current()->label.prim);
	lv_obj_align(label_txt3, label_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, -LV_DPI / 7);

	line_sep = lv_line_create(h2, line_sep);
	lv_obj_align(line_sep, label_txt3, LV_ALIGN_OUT_BOTTOM_LEFT, -(LV_DPI / 2), LV_DPI / 8);
	lv_line_set_style(line_sep, lv_theme_get_current()->line.decor);

	lv_obj_t *list_sd_based = lv_list_create(h2, NULL);
	lv_obj_align(list_sd_based, line_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 2, LV_DPI / 4);

	lv_obj_set_size(list_sd_based, LV_HOR_RES * 4 / 10, LV_VER_RES * 6 / 10);
	lv_list_set_single_mode(list_sd_based, true);

	if (!emummc_img->dirlist)
		goto out1;

	emummc_idx = 0;

	// Add file based to the list.
	while (emummc_img->dirlist->name[emummc_idx])
	{
		s_printf(path, "emuMMC/%s", emummc_img->dirlist->name[emummc_idx]);

		lv_list_add(list_sd_based, NULL, path, _save_file_emummc_cfg_action);

		emummc_idx++;
	}

out1:
	free(path);
	sd_unmount();

	return LV_RES_OK;
}

lv_res_t create_win_emummc_tools(lv_obj_t *btn)
{
	lv_obj_t *win = nyx_create_nand_manager_window(SYMBOL_SETTINGS"  낸드 매니저");
	//==================================================
	//  ASAP: PIN Lock setup, Package1/2 dump buttons.
	//==================================================
	lv_win_add_btn(win, NULL, "Ｏ PIN", _action_win_nyx_options_passwd);
	lv_win_add_btn(win, NULL, SYMBOL_SAVE" PKG1/2", _create_window_dump_pk12_tool);
	//==================================================

	// Set resources to be managed by other windows.
	emummc_manage_window = win;
	emummc_tools = (void *)create_win_emummc_tools;

	sd_mount();

	emummc_cfg_t emu_info;
	load_emummc_cfg(&emu_info);

	sd_unmount();

	static lv_style_t h_style;
	lv_style_copy(&h_style, &lv_style_transp);
	h_style.body.padding.inner = 0;
	h_style.body.padding.hor = LV_DPI - (LV_DPI / 4);
	h_style.body.padding.ver = LV_DPI / 9;

	// Create emuMMC Info & Selection container.
	lv_obj_t *h1 = lv_cont_create(win, NULL);
	lv_cont_set_style(h1, &h_style);
	lv_cont_set_fit(h1, false, true);
	lv_obj_set_width(h1, (LV_HOR_RES / 9) * 4);
	lv_obj_set_click(h1, false);
	lv_cont_set_layout(h1, LV_LAYOUT_OFF);

	lv_obj_t *label_sep = lv_label_create(h1, NULL);
	lv_label_set_static_text(label_sep, "");

	lv_obj_t *label_txt = lv_label_create(h1, NULL);
	lv_label_set_static_text(label_txt, SYMBOL_INFO"  낸드 저장소 상태 "SYMBOL_DOT" 정보 "SYMBOL_DOT" 변경");
	lv_obj_set_style(label_txt, lv_theme_get_current()->label.prim);
	lv_obj_align(label_txt, label_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, -LV_DPI / 9);

	lv_obj_t *line_sep = lv_line_create(h1, NULL);
	static const lv_point_t line_pp[] = { {0, 0}, { LV_HOR_RES - (LV_DPI - (LV_DPI / 4)) * 2, 0} };
	lv_line_set_points(line_sep, line_pp, 2);
	lv_line_set_style(line_sep, lv_theme_get_current()->line.decor);
	lv_obj_align(line_sep, label_txt, LV_ALIGN_OUT_BOTTOM_LEFT, -(LV_DPI / 4), LV_DPI / 8);

	// Create NAND info labels.
	lv_obj_t *label_btn = lv_label_create(h1, NULL);
	lv_label_set_recolor(label_btn, true);
	lv_label_set_static_text(label_btn, "");
	lv_obj_align(label_btn, line_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, LV_DPI / 4);

	//================================================
	//  ASAP: NAND informations toggle label button.
	//================================================
	lv_obj_t *btn_nandinfo = lv_btn_create(h1, NULL);
	lv_btn_set_fit(btn_nandinfo, true, true);
	label_btn = lv_label_create(btn_nandinfo, NULL);
	lv_label_set_static_text(label_btn, "            ");
	lv_obj_align(btn_nandinfo, line_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, LV_DPI / 4);
	lv_btn_set_action(btn_nandinfo, LV_BTN_ACTION_CLICK, emu_info.enabled ? _create_window_sdcard_info_status : _create_window_emmc_info_status);
	lv_obj_t *lbl_nandinfo = lv_label_create(h1, NULL);
	lv_label_set_recolor(lbl_nandinfo, true);
	lv_label_set_static_text(lbl_nandinfo, emu_info.enabled ? "#00FFCC Ⓢ 에뮤낸드#" : "#FF8000 Ⓝ 시스낸드#");
	lv_obj_align(lbl_nandinfo, btn_nandinfo, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t *btn_elnandinfo = lv_btn_create(h1, NULL);
	label_btn = lv_label_create(btn_elnandinfo, NULL);
	lv_btn_set_fit(btn_elnandinfo, true, true);
	lv_label_set_static_text(label_btn, "    ");
	lv_obj_align(btn_elnandinfo, btn_nandinfo, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_elnandinfo, LV_BTN_ACTION_CLICK, emu_info.enabled ? _create_window_emmc_info_status : _create_window_sdcard_info_status);
	lv_obj_t *lbl_elnandinfo = lv_label_create(h1, NULL);
	lv_label_set_recolor(lbl_elnandinfo, true);
	lv_label_set_static_text(lbl_elnandinfo, emu_info.enabled ? "Ⓝ" : "Ⓢ");
	lv_obj_align(lbl_elnandinfo, btn_elnandinfo, LV_ALIGN_CENTER, 0, 0);

	// Create Change NAND button.
	lv_obj_t *btn_change_nand = lv_btn_create(h1, NULL);
	label_btn = lv_label_create(btn_change_nand, NULL);
	lv_btn_set_fit(btn_change_nand, true, true);
	lv_label_set_static_text(label_btn, SYMBOL_REFRESH" 변경");
	lv_obj_align(btn_change_nand, btn_elnandinfo, LV_ALIGN_OUT_RIGHT_MID, 30, 0);
	lv_btn_set_action(btn_change_nand, LV_BTN_ACTION_CLICK, _create_change_emummc_window);
	//================================================

	lv_obj_t *label_txt2 = lv_label_create(h1, NULL);
	lv_label_set_recolor(label_txt2, true);
	char *txt_buf = (char *)malloc(SZ_16K);

	if (emu_info.enabled)
	{
		if (emu_info.sector)
			s_printf(txt_buf, "#00DDFF 유형:# 파티션 (0x%08X)\n#00DDFF 경로:# sdmc:/%s",
				emu_info.sector, emu_info.path ? emu_info.path : "");
		else
			s_printf(txt_buf, "#00DDFF 유형:# 파일\n#00DDFF 경로:# sdmc:/%s",
				emu_info.path ? emu_info.path : "");

		lv_label_set_text(label_txt2, txt_buf);
	}
	else
	{
		lv_label_set_static_text(label_txt2, "#00DDFF 유형:# eMMC\n#00DDFF 경로:# sdmc:/Nintendo");
	}

	if (emu_info.path)
		free(emu_info.path);
	// Nintendo folder path.
	/* if (emu_info.nintendo_path)
		free(emu_info.nintendo_path); */
	free(txt_buf);

	//============================
	//  ASAP: Change info label.
	//============================
	lv_obj_set_style(label_txt2, &hint_small_style);
	lv_obj_align(label_txt2, btn_nandinfo, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 3);

	lv_obj_t *label_txt3 = lv_label_create(h1, NULL);
	lv_label_set_recolor(label_txt3, true);
	lv_label_set_static_text(label_txt3, "#00DDFF 변경:# 사용하려는 #C7EA46 낸드 유형#을 변경합니다.");
	lv_obj_set_style(label_txt3, &hint_small_style);
	lv_obj_align(label_txt3, label_txt2, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

	//===========================================================
	//  ASAP: New Header: Partition, Create, Migration manager.
	//===========================================================
	lv_obj_t *pcm_manager = lv_label_create(h1, NULL);
	lv_label_set_static_text(pcm_manager, SYMBOL_DRIVE"  시스·에뮤낸드 - 포맷·분할 "SYMBOL_DOT" 생성 "SYMBOL_DOT" 이사");
	lv_obj_set_style(pcm_manager, lv_theme_get_current()->label.prim);
	lv_obj_align(pcm_manager, label_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, LV_DPI * 2.6);
	line_sep = lv_line_create(h1, line_sep);
	lv_obj_align(line_sep, pcm_manager, LV_ALIGN_OUT_BOTTOM_LEFT, -(LV_DPI / 4), LV_DPI / 8);

	// Create Partition Manager button.
	lv_obj_t *btn_part_mng = lv_btn_create(h1, NULL);
	label_btn = lv_label_create(btn_part_mng, NULL);
	lv_btn_set_fit(btn_part_mng, true, true);
	lv_label_set_static_text(label_btn, SYMBOL_COPY" 포맷·분할");
	lv_obj_align(btn_part_mng, pcm_manager, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 2.5);
	lv_btn_set_action(btn_part_mng, LV_BTN_ACTION_CLICK, create_window_sd_partition_manager);
	lv_btn_set_action(btn_part_mng, LV_BTN_ACTION_LONG_PR, create_window_emmc_partition_manager);

	// Create emuMMC Manager button.
	lv_obj_t *btn_mmc_crt_mng = lv_btn_create(h1, NULL);
	label_btn = lv_label_create(btn_mmc_crt_mng, NULL);
	lv_btn_set_fit(btn_mmc_crt_mng, true, true);
	lv_label_set_static_text(label_btn, SYMBOL_SD" 생성");
	lv_obj_align(btn_mmc_crt_mng, btn_part_mng, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_mmc_crt_mng, LV_BTN_ACTION_CLICK, _create_mbox_emummc_create);

	// Create Migration Manager button.
	lv_obj_t *btn_mig_mng = lv_btn_create(h1, NULL);
	label_btn = lv_label_create(btn_mig_mng, NULL);
	lv_btn_set_fit(btn_mig_mng, true, true);
	lv_label_set_static_text(label_btn, SYMBOL_SHUFFLE" 이사");
	lv_obj_align(btn_mig_mng, btn_mmc_crt_mng, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_mig_mng, LV_BTN_ACTION_CLICK, _create_mbox_emummc_migrate);

	label_txt2 = lv_label_create(h1, NULL);
	lv_label_set_recolor(label_txt2, true);
	lv_label_set_static_text(label_txt2,
		"#00DDFF 포맷·분할:# #C7EA46 eMMC#, #C7EA46 SD FAT32# 포맷 혹은 #C7EA46 L4T#용 파티션을 분할합니다.\n"
		"#FF8000            3초 이상 입력 유지 시, eMMC를 분할 가능하나 모두 포맷됩니다.#\n\n"
		"#00DDFF 생성:# #C7EA46 파티션# 혹은 #C7EA46 파일# 에뮤낸드를 생성합니다.\n"
		"#00DDFF 이사:# 백업한 #C7EA46 파티션#을 #C7EA46 파일#로 변경하거나, #FF8000 SXOS#에서 이사합니다.");

	lv_obj_set_style(label_txt2, &hint_small_style);
	lv_obj_align(label_txt2, btn_part_mng, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 3);

	//=================================================================================
	//  ASAP: Create NAND Backup & Restore & Data verification, Connection container.
	//=================================================================================
	lv_obj_t *h2 = lv_cont_create(win, NULL);
	lv_cont_set_style(h2, &h_style);
	lv_cont_set_fit(h2, false, true);
	lv_obj_set_width(h2, (LV_HOR_RES / 9) * 4);
	lv_obj_set_click(h2, false);
	lv_cont_set_layout(h2, LV_LAYOUT_OFF);
	lv_obj_align(h2, h1, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI * 17 / 29, 0);

	label_sep = lv_label_create(h2, NULL);
	lv_label_set_static_text(label_sep, "");

	lv_obj_t *label_txt4 = lv_label_create(h2, NULL);
	lv_label_set_static_text(label_txt4, SYMBOL_TOOLS"  백업·복원 "SYMBOL_DOT" 데이터 검사 "SYMBOL_DOT" PC 연결");
	lv_obj_set_style(label_txt4, lv_theme_get_current()->label.prim);
	lv_obj_align(label_txt4, label_sep, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI / 4, 0);

	line_sep = lv_line_create(h2, line_sep);
	lv_obj_align(line_sep, label_txt4, LV_ALIGN_OUT_BOTTOM_LEFT, -(LV_DPI / 4), LV_DPI / 8);

	// Create Backup NAND button.
	lv_obj_t *btn_bkupmmc = lv_btn_create(h2, NULL);
	label_btn = lv_label_create(btn_bkupmmc, NULL);
	lv_btn_set_fit(btn_bkupmmc, true, true);
	lv_label_set_static_text(label_btn, SYMBOL_UPLOAD" 낸드 백업");
	lv_obj_align(btn_bkupmmc, label_txt4, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 2.5);
	lv_btn_set_action(btn_bkupmmc, LV_BTN_ACTION_CLICK, create_window_backup_restore_tool);

	// Create Restore NAND button.
	lv_obj_t *btn_rstrmmc = lv_btn_create(h2, NULL);
	label_btn = lv_label_create(btn_rstrmmc, NULL);
	lv_btn_set_fit(btn_rstrmmc, true, true);
	lv_label_set_static_text(label_btn, SYMBOL_DOWNLOAD" 낸드 복원");
	lv_obj_align(btn_rstrmmc, btn_bkupmmc, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_rstrmmc, LV_BTN_ACTION_CLICK, create_window_backup_restore_tool);
	
	// Data verification dropdown list and info label.
	lv_obj_t *ddlist = lv_ddlist_create(h2, NULL);
	lv_obj_set_top(ddlist, true);
	lv_ddlist_set_draw_arrow(ddlist, true);
	lv_ddlist_set_options(ddlist,
		"스킵    \n"
		"부분\n"
		"전체\n"
		"해시");
	lv_ddlist_set_selected(ddlist, n_cfg.verification);
	lv_obj_align(ddlist, btn_rstrmmc, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_ddlist_set_action(ddlist, _data_verification_action);

	lv_obj_t *label_txt5 = lv_label_create(h2, NULL);
	lv_label_set_recolor(label_txt5, true);
	lv_label_set_static_text(label_txt5,
		"낸드의 #C7EA46 BOOT0#, #C7EA46 BOOT1#, #C7EA46 RAW GPP# 파티션을 백업·복원합니다.\n"
		"#C7EA46 FAT32#, #C7EA46 exFAT#으로 포맷된 최소 #FF8000 4GB# 이상의 SD 카드가 필요합니다.\n"
		"#00DDFF 데이터 검사 (속도):# 스킵 (#0098FE 고속#), 부분 (#C7EA46 빠름#), 전체 (#FF8000 보통#), 해시 (#FF8000 느림#)");

	lv_obj_set_style(label_txt5, &hint_small_style);
	lv_obj_align(label_txt5, btn_bkupmmc, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 3);

	// Create eMMC GPP UMS buttons.
	lv_obj_t *btn_sys_gpp = lv_btn_create(h2, btn_mig_mng);
	label_btn = lv_label_create(btn_sys_gpp, NULL);
	lv_label_set_static_text(label_btn, SYMBOL_USB" 시스낸드");
	lv_obj_align(btn_sys_gpp, label_txt5, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 2.5);
	lv_btn_set_action(btn_sys_gpp, LV_BTN_ACTION_CLICK, _action_ums_emmc_gpp);

	// Create eMMC BOOT0 UMS button.
	lv_obj_t *btn_boot0 = lv_btn_create(h2, btn_mig_mng);
	label_btn = lv_label_create(btn_boot0, NULL);
	lv_label_set_static_text(label_btn, "BOOT0");
	lv_obj_align(btn_boot0, btn_sys_gpp, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_boot0, LV_BTN_ACTION_CLICK, _action_ums_emmc_boot0);

	// Create eMMC BOOT1 UMS button.
	lv_obj_t *btn_boot1 = lv_btn_create(h2, btn_mig_mng);
	label_btn = lv_label_create(btn_boot1, NULL);
	lv_label_set_static_text(label_btn, "BOOT1");
	lv_obj_align(btn_boot1, btn_boot0, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_boot1, LV_BTN_ACTION_CLICK, _action_ums_emmc_boot1);

	// Create emuMMC RAW GPP UMS button.
	lv_obj_t *btn_emu_gpp = lv_btn_create(h2, btn_mig_mng);
	label_btn = lv_label_create(btn_emu_gpp, NULL);
	lv_label_set_static_text(label_btn, SYMBOL_USB" 에뮤낸드");
	lv_obj_align(btn_emu_gpp, btn_sys_gpp, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
	lv_btn_set_action(btn_emu_gpp, LV_BTN_ACTION_CLICK, _action_ums_emuemmc_gpp);

	// Create emuMMC BOOT0 UMS button.
	lv_obj_t *btn_emu_boot0 = lv_btn_create(h2, btn_mig_mng);
	label_btn = lv_label_create(btn_emu_boot0, NULL);
	lv_label_set_static_text(label_btn, "BOOT0");
	lv_obj_align(btn_emu_boot0, btn_emu_gpp, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_emu_boot0, LV_BTN_ACTION_CLICK, _action_ums_emuemmc_boot0);

	// Create emuMMC BOOT1 UMS button.
	lv_obj_t *btn_emu_boot1 = lv_btn_create(h2, btn_mig_mng);
	label_btn = lv_label_create(btn_emu_boot1, NULL);
	lv_label_set_static_text(label_btn, "BOOT1");
	lv_obj_align(btn_emu_boot1, btn_emu_boot0, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
	lv_btn_set_action(btn_emu_boot1, LV_BTN_ACTION_CLICK, _action_ums_emuemmc_boot1);

	// PC Connection info label.
	lv_obj_t *label_txt6 = lv_label_create(h2, NULL);
	lv_label_set_recolor(label_txt6, true);
	lv_label_set_static_text(label_txt6,
		"#C7EA46 Linux#, #C7EA46 HacDiskMount#, #C7EA46 NxNandManager# 등을 이용하여\n"
		"낸드의 #C7EA46 RAW GPP#, #C7EA46 BOOT0#, #C7EA46 BOOT1# 파티션을 #FF8000 PC#에 마운트합니다.\n");
	lv_obj_set_style(label_txt6, &hint_small_style);
	lv_obj_align(label_txt6, btn_emu_gpp, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 3);

	// Create read/write access toggle button and info label.
	lv_obj_t *label_txt7 = lv_label_create(h2, NULL);
	lv_label_set_recolor(label_txt7, true);
	lv_label_set_static_text(label_txt7,
		"#00DDFF 읽기:# 쓰기 보호 #0098FE ON#\n"
		"#00DDFF 쓰기:# 쓰기 보호 OFF             ");
	lv_obj_set_style(label_txt7, &hint_small_style);
	lv_obj_align(label_txt7, label_txt6, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

	lv_obj_t *h_write = lv_cont_create(win, NULL);
	lv_cont_set_style(h_write, &h_style);
	lv_cont_set_fit(h_write, false, true);
	lv_obj_set_width(h_write, (LV_HOR_RES / 9) * 2);
	lv_obj_set_click(h_write, false);
	lv_cont_set_layout(h_write, LV_LAYOUT_OFF);
	lv_obj_align(h_write, label_txt7, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 10, 0);

	lv_obj_t *btn_write_access = lv_btn_create(h_write, NULL);
	nyx_create_onoff_button(lv_theme_get_current(), h_write,
		btn_write_access, SYMBOL_EDIT" 쓰기 보호", _emmc_read_only_toggle, false);
	if (!n_cfg.ums_emmc_rw)
		lv_btn_set_state(btn_write_access, LV_BTN_STATE_TGL_REL);
	_emmc_read_only_toggle(btn_write_access);

	return LV_RES_OK;
}
