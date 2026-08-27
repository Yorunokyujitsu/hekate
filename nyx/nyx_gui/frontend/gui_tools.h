/*
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

#ifndef _GUI_TOOLS_H_
#define _GUI_TOOLS_H_

#include <libs/lvgl/lvgl.h>

extern lv_obj_t *ums_mbox;

void nyx_run_ums(void *param);
bool get_set_autorcm_status(bool change);
lv_res_t action_ums_sd(lv_obj_t *btn);

//=====================================================
//  ASAP: BOOT, GPP, R/W toggle. (gui_emummc_tools.c)
//=====================================================
lv_res_t _action_ums_emmc_gpp(lv_obj_t *btn);
lv_res_t _action_ums_emmc_boot0(lv_obj_t *btn);
lv_res_t _action_ums_emmc_boot1(lv_obj_t *btn);
lv_res_t _action_ums_emuemmc_gpp(lv_obj_t *btn);
lv_res_t _action_ums_emuemmc_boot0(lv_obj_t *btn);
lv_res_t _action_ums_emuemmc_boot1(lv_obj_t *btn);
lv_res_t _emmc_read_only_toggle(lv_obj_t *btn);
lv_res_t _create_window_dump_pk12_tool(lv_obj_t *btn);
//==========================================================
//  ASAP: Display touchscreen coordinate fix. (gui_info.c)
//==========================================================
lv_res_t _create_mbox_fix_touchscreen(lv_obj_t *btn);
//==================================================
//  ASAP: Auto RCM toggle and HID Joy-con. (gui.c)
//==================================================
lv_res_t _create_mbox_autorcm_status(lv_obj_t *btn);
lv_res_t _action_hid_jc(lv_obj_t *btn);
//=============================================
//  ASAP: Archivebit fix action. (gui_info.c)
//=============================================
lv_res_t _create_window_unset_abit_tool(lv_obj_t *btn);
//=====================================================

#endif
