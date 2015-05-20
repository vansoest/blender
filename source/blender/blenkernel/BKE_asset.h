/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BKE_asset.h
 *  \ingroup bke
 */

#ifndef __BKE_ASSET_H__
#define __BKE_ASSET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_space_types.h"

struct AssetEngine;
struct AssetEngineType;
struct AssetUUIDList;
struct FileDirEntryArr;
struct FileDirEntry;
struct FileDirEntryVariant;
struct FileDirEntryRevision;
struct ExtensionRNA;
struct ID;
struct IDProperty;
struct ListBase;
struct uiLayout;

enum {
	AE_STATUS_VALID   = 1 << 0,
	AE_STATUS_RUNNING = 1 << 1,  /* Asset engine is performing some background tasks... */
};

#define AE_FAKE_ENGINE_ID "NONE"

extern ListBase asset_engines;

/* AE instance/job is valid, is running, is idle, etc. */
typedef int (*ae_status)(struct AssetEngine *engine, const int job_id);
typedef float (*ae_progress)(struct AssetEngine *engine, const int job_id);

/* To force end of given job (e.g. because it was cancelled by user...). */
typedef void (*ae_kill)(struct AssetEngine *engine, const int job_id);

/* ***** All callbacks below shall be non-blocking (i.e. return immediately). ***** */
/* Those callbacks will be called from a 'fake-job' start *and* update functions (i.e. main thread, working one will
 * just sleep).
 * If given id is not null, engine should update from a running job if available, otherwise it should start a new one.
 * It is the responsability of the engine to start/stop background processes to actually perform tasks as/if needed.
 */

/* List everything available at given root path - only returns numbers of entries! */
typedef int (*ae_list_dir)(struct AssetEngine *engine, const int job_id, struct FileDirEntryArr *entries_r);

/* Ensure given direntries are really available for append/link (some kind of 'anticipated loading'...). */
typedef int (*ae_ensure_entries)(struct AssetEngine *engine, const int job_id, struct AssetUUIDList *uuids);

/* ***** All callbacks below are blocking. They shall be completed upon return. ***** */

/* Perform sorting and/or filtering on engines' side.
 * Note that engine is assumed to feature its own sorting/filtering settings!
 * Number of available filtered entries is to be set in entries_r.
 */
typedef bool (*ae_sort_filter)(struct AssetEngine *engine, const bool sort, const bool filter,
                               struct FileSelectParams *params, struct FileDirEntryArr *entries_r);

/* Return specified block of entries in entries_r. */
typedef bool (*ae_entries_block_get)(struct AssetEngine *engine, const int start_index, const int end_index,
                                     struct FileDirEntryArr *entries_r);

/* Return specified entries from their uuids, in entries_r. */
typedef bool (*ae_entries_uuid_get)(struct AssetEngine *engine, struct AssetUUIDList *uuids,
                                    struct FileDirEntryArr *entries_r);

/* 'pre-loading' hook, called before opening/appending/linking given entries.
 * Note first given uuid is the one of 'active' entry, and first entry in returned list will be considered as such too.
 * E.g. allows the engine to ensure entries' paths are actually valid by downloading requested data, etc.
 * If is_virtual is True, then there is no requirement that returned paths actually exist.
 * Note that the generated list shall be simpler than the one generated by ae_list_dir, since only the path from
 * active revision is used, no need to bother with variants, previews, etc.
 * This allows to present 'fake' entries to user, and then import actual data.
 */
typedef bool (*ae_load_pre)(struct AssetEngine *engine, struct AssetUUIDList *uuids,
                            struct FileDirEntryArr *entries_r);

/* 'post-loading' hook, called after opening/appending/linking given entries.
 * E.g. allows an advanced engine to make fancy scripted operations over loaded items. */
typedef bool (*ae_load_post)(struct AssetEngine *engine, struct ID *items, const int *num_items);

typedef struct AssetEngineType {
	struct AssetEngineType *next, *prev;

	/* type info */
	char idname[64]; /* best keep the same size as BKE_ST_MAXNAME */
	char name[64];
	int flag;

	/* API */
	ae_status status;
	ae_progress progress;

	ae_kill kill;

	ae_list_dir list_dir;
	ae_sort_filter sort_filter;
	ae_entries_block_get entries_block_get;
	ae_entries_uuid_get entries_uuid_get;
	ae_ensure_entries ensure_entries;

	ae_load_pre load_pre;
	ae_load_post load_post;

	/* RNA integration */
	struct ExtensionRNA ext;
} AssetEngineType;

typedef struct AssetEngine {
	AssetEngineType *type;
	void *py_instance;

	/* Custom sub-classes properties. */
	IDProperty *properties;

	int flag;
	int refcount;

	struct ReportList *reports;
} AssetEngine;

/* AssetEngine->flag */
enum {
	AE_DIRTY_FILTER  = 1 << 0,
	AE_DIRTY_SORTING = 1 << 1,
};

/* Engine Types */
void BKE_asset_engines_init(void);
void BKE_asset_engines_exit(void);

AssetEngineType *BKE_asset_engines_find(const char *idname);

/* Engine Instances */
AssetEngine *BKE_asset_engine_create(AssetEngineType *type);
AssetEngine *BKE_asset_engine_copy(AssetEngine *engine);
void BKE_asset_engine_free(AssetEngine *engine);

AssetUUIDList *BKE_asset_engine_load_pre(AssetEngine *engine, struct FileDirEntryArr *r_entries);

/* File listing utils... */

typedef enum FileCheckType {
	CHECK_NONE  = 0,
	CHECK_DIRS  = 1 << 0,
	CHECK_FILES = 1 << 1,
	CHECK_ALL   = CHECK_DIRS | CHECK_FILES,
} FileCheckType;

void BKE_filedir_revision_free(struct FileDirEntryRevision *rev);

void BKE_filedir_variant_free(struct FileDirEntryVariant *var);

void BKE_filedir_entry_free(struct FileDirEntry *entry);
void BKE_filedir_entry_clear(struct FileDirEntry *entry);
struct FileDirEntry *BKE_filedir_entry_copy(struct FileDirEntry *entry);

void BKE_filedir_entryarr_clear(struct FileDirEntryArr *array);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_ASSET_H__ */
