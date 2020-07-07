#include "TexturePack.h"
#include "Constants.h"
#include "Stream.h"
#include "World.h"
#include "Graphics.h"
#include "Event.h"
#include "Game.h"
#include "Http.h"
#include "Platform.h"
#include "Deflate.h"
#include "Funcs.h"
#include "ExtMath.h"
#include "Options.h"
#include "Logger.h"
#include "Chat.h" /* TODO avoid this include */

/*########################################################################################################################*
*------------------------------------------------------TerrainAtlas-------------------------------------------------------*
*#########################################################################################################################*/
struct _Atlas2DData Atlas2D;
struct _Atlas1DData Atlas1D;

TextureRec Atlas1D_TexRec(TextureLoc texLoc, int uCount, int* index) {
	TextureRec rec;
	int y  = Atlas1D_RowId(texLoc);
	*index = Atlas1D_Index(texLoc);

	/* Adjust coords to be slightly inside - fixes issues with AMD/ATI cards */	
	rec.U1 = 0.0f; 
	rec.V1 = y * Atlas1D.InvTileSize;
	rec.U2 = (uCount - 1) + UV2_Scale;
	rec.V2 = rec.V1       + UV2_Scale * Atlas1D.InvTileSize;
	return rec;
}

static void Atlas_Convert2DTo1D(void) {
	int tileSize      = Atlas2D.TileSize;
	int tilesPerAtlas = Atlas1D.TilesPerAtlas;
	int atlasesCount  = Atlas1D.Count;
	Bitmap atlas1D;
	int atlasX, atlasY;
	int tile = 0, i, y;

	Platform_Log2("Loaded new atlas: %i bmps, %i per bmp", &atlasesCount, &tilesPerAtlas);
	Bitmap_Allocate(&atlas1D, tileSize, tilesPerAtlas * tileSize);
	
	for (i = 0; i < atlasesCount; i++) {
		for (y = 0; y < tilesPerAtlas; y++, tile++) {
			atlasX = Atlas2D_TileX(tile) * tileSize;
			atlasY = Atlas2D_TileY(tile) * tileSize;

			Bitmap_UNSAFE_CopyBlock(atlasX, atlasY, 0, y * tileSize,
							&Atlas2D.Bmp, &atlas1D, tileSize);
		}
		Atlas1D.TexIds[i] = Gfx_CreateTexture(&atlas1D, true, Gfx.Mipmaps);
	}
	Mem_Free(atlas1D.scan0);
}

static void Atlas_Update1D(void) {
	int maxAtlasHeight, maxTilesPerAtlas, maxTiles;

	maxAtlasHeight   = min(4096, Gfx.MaxTexHeight);
	maxTilesPerAtlas = maxAtlasHeight / Atlas2D.TileSize;
	maxTiles         = Atlas2D.RowsCount * ATLAS2D_TILES_PER_ROW;

	Atlas1D.TilesPerAtlas = min(maxTilesPerAtlas, maxTiles);
	Atlas1D.Count = Math_CeilDiv(maxTiles, Atlas1D.TilesPerAtlas);

	Atlas1D.InvTileSize = 1.0f / Atlas1D.TilesPerAtlas;
	Atlas1D.Mask  = Atlas1D.TilesPerAtlas - 1;
	Atlas1D.Shift = Math_Log2(Atlas1D.TilesPerAtlas);
}

/* Loads the given atlas and converts it into an array of 1D atlases. */
static void Atlas_Update(Bitmap* bmp) {
	Atlas2D.Bmp       = *bmp;
	Atlas2D.TileSize  = bmp->width  / ATLAS2D_TILES_PER_ROW;
	Atlas2D.RowsCount = bmp->height / Atlas2D.TileSize;
	Atlas2D.RowsCount = min(Atlas2D.RowsCount, ATLAS2D_MAX_ROWS_COUNT);

	Atlas_Update1D();
	Atlas_Convert2DTo1D();
}

static GfxResourceID Atlas_LoadTile_Raw(TextureLoc texLoc, Bitmap* element) {
	int size = Atlas2D.TileSize;
	int x = Atlas2D_TileX(texLoc), y = Atlas2D_TileY(texLoc);
	if (y >= Atlas2D.RowsCount) return 0;

	Bitmap_UNSAFE_CopyBlock(x * size, y * size, 0, 0, &Atlas2D.Bmp, element, size);
	return Gfx_CreateTexture(element, false, Gfx.Mipmaps);
}

GfxResourceID Atlas2D_LoadTile(TextureLoc texLoc) {
	BitmapCol pixels[64 * 64];
	int size = Atlas2D.TileSize;
	Bitmap tile;
	GfxResourceID texId;

	/* Try to allocate bitmap on stack if possible */
	if (size > 64) {
		Bitmap_Allocate(&tile, size, size);
		texId = Atlas_LoadTile_Raw(texLoc, &tile);
		Mem_Free(tile.scan0);
		return texId;
	} else {	
		Bitmap_Init(tile, size, size, pixels);
		return Atlas_LoadTile_Raw(texLoc, &tile);
	}
}

static void Atlas2D_Free(void) {
	Mem_Free(Atlas2D.Bmp.scan0);
	Atlas2D.Bmp.scan0 = NULL;
}

static void Atlas1D_Free(void) {
	int i;
	for (i = 0; i < Atlas1D.Count; i++) {
		Gfx_DeleteTexture(&Atlas1D.TexIds[i]);
	}
}

cc_bool Atlas_TryChange(Bitmap* atlas) {
	static const String terrain = String_FromConst("terrain.png");
	if (!Game_ValidateBitmap(&terrain, atlas)) return false;

	if (atlas->height < atlas->width) {
		Chat_AddRaw("&cUnable to use terrain.png from the texture pack.");
		Chat_AddRaw("&c Its height is less than its width.");
		return false;
	}
	if (atlas->width < ATLAS2D_TILES_PER_ROW) {
		Chat_AddRaw("&cUnable to use terrain.png from the texture pack.");
		Chat_AddRaw("&c It must be 16 or more pixels wide.");
		return false;
	}

	if (Gfx.LostContext) return false;
	Atlas1D_Free();
	Atlas2D_Free();

	Atlas_Update(atlas);
	Event_RaiseVoid(&TextureEvents.AtlasChanged);
	return true;
}


/*########################################################################################################################*
*------------------------------------------------------TextureCache-------------------------------------------------------*
*#########################################################################################################################*/
static struct StringsBuffer acceptedList, deniedList, etagCache, lastModCache;
#define ACCEPTED_TXT "texturecache/acceptedurls.txt"
#define DENIED_TXT   "texturecache/deniedurls.txt"
#define ETAGS_TXT    "texturecache/etags.txt"
#define LASTMOD_TXT  "texturecache/lastmodified.txt"

/* Initialises cache state (loading various lists) */
static void TextureCache_Init(void) {
	EntryList_UNSAFE_Load(&acceptedList, ACCEPTED_TXT);
	EntryList_UNSAFE_Load(&deniedList,   DENIED_TXT);
	EntryList_UNSAFE_Load(&etagCache,    ETAGS_TXT);
	EntryList_UNSAFE_Load(&lastModCache, LASTMOD_TXT);
}

cc_bool TextureCache_HasAccepted(const String* url) { return EntryList_Find(&acceptedList, url, ' ') >= 0; }
cc_bool TextureCache_HasDenied(const String* url)   { return EntryList_Find(&deniedList,   url, ' ') >= 0; }

void TextureCache_Accept(const String* url) { 
	EntryList_Set(&acceptedList, url, &String_Empty, ' '); 
	EntryList_Save(&acceptedList, ACCEPTED_TXT);
}
void TextureCache_Deny(const String* url) { 
	EntryList_Set(&deniedList,  url, &String_Empty, ' '); 
	EntryList_Save(&deniedList, DENIED_TXT);
}

int TextureCache_ClearDenied(void) {
	int count = deniedList.count;
	StringsBuffer_Clear(&deniedList);
	EntryList_Save(&deniedList, DENIED_TXT);
	return count;
}

CC_INLINE static void HashUrl(String* key, const String* url) {
	String_AppendUInt32(key, Utils_CRC32((const cc_uint8*)url->buffer, url->length));
}

CC_NOINLINE static void MakeCachePath(String* path, const String* url) {
	String key; char keyBuffer[STRING_INT_CHARS];
	String_InitArray(key, keyBuffer);

	HashUrl(&key, url);
	String_Format1(path, "texturecache/%s", &key);
}

/* Returns non-zero if given URL has been cached */
static int IsCached(const String* url) {
	String path; char pathBuffer[FILENAME_SIZE];
	String_InitArray(path, pathBuffer);

	MakeCachePath(&path, url);
	return File_Exists(&path);
}

/* Attempts to open the cached data stream for the given url */
static cc_bool OpenCachedData(const String* url, struct Stream* stream) {
	String path; char pathBuffer[FILENAME_SIZE];
	cc_result res;

	String_InitArray(path, pathBuffer);
	MakeCachePath(&path, url);
	res = Stream_OpenFile(stream, &path);

	if (res == ReturnCode_FileNotFound) return false;
	if (res) { Logger_Warn2(res, "opening cache for", url); return false; }
	return true;
}

CC_NOINLINE static String GetCachedTag(const String* url, struct StringsBuffer* list) {
	String key; char keyBuffer[STRING_INT_CHARS];
	String_InitArray(key, keyBuffer);

	HashUrl(&key, url);
	return EntryList_UNSAFE_Get(list, &key, ' ');
}

static String GetCachedLastModified(const String* url) {
	int i;
	String entry = GetCachedTag(url, &lastModCache);
	/* Entry used to be a timestamp of C# ticks since 01/01/0001 */
	/* Check if this is new format */
	for (i = 0; i < entry.length; i++) {
		if (entry.buffer[i] < '0' || entry.buffer[i] > '9') return entry;
	}

	/* Entry is all digits, so the old unsupported format */
	entry.length = 0; return entry;
}

static String GetCachedETag(const String* url) {
	return GetCachedTag(url, &etagCache);
}

CC_NOINLINE static void SetCachedTag(const String* url, struct StringsBuffer* list,
									 const String* data, const char* file) {
	String key; char keyBuffer[STRING_INT_CHARS];
	if (!data->length) return;

	String_InitArray(key, keyBuffer);
	HashUrl(&key, url);
	EntryList_Set(list, &key, data, ' ');
	EntryList_Save(list, file);
}

/* Updates cached data, ETag, and Last-Modified for the given URL */
static void UpdateCache(struct HttpRequest* req) {
	String path, url; char pathBuffer[FILENAME_SIZE];
	cc_result res;
	url = String_FromRawArray(req->url);

	path = String_FromRawArray(req->etag);
	SetCachedTag(&url, &etagCache, &path, ETAGS_TXT);
	path = String_FromRawArray(req->lastModified);
	SetCachedTag(&url, &lastModCache, &path, LASTMOD_TXT);

	String_InitArray(path, pathBuffer);
	MakeCachePath(&path, &url);
	res = Stream_WriteAllTo(&path, req->data, req->size);
	if (res) { Logger_Warn2(res, "caching", &url); }
}


/*########################################################################################################################*
*-------------------------------------------------------TexturePack-------------------------------------------------------*
*#########################################################################################################################*/
static char defTexPackBuffer[STRING_SIZE];
static String defTexPack = String_FromArray(defTexPackBuffer);
static const String defaultZip = String_FromConst("default.zip");

/* Retrieves the filename of the default texture pack used. */
/* NOTE: Returns default.zip if classic mode or selected pack does not exist. */
static String TexturePack_UNSAFE_GetDefault(void) {
	String texPath; char texPathBuffer[STRING_SIZE];
	String_InitArray(texPath, texPathBuffer);

	String_Format1(&texPath, "texpacks/%s", &defTexPack);
	return File_Exists(&texPath) && !Game_ClassicMode ? defTexPack : defaultZip;
}

void TexturePack_SetDefault(const String* texPack) {
	String_Copy(&defTexPack, texPack);
	Options_Set(OPT_DEFAULT_TEX_PACK, texPack);
}

static cc_result TexturePack_ProcessZipEntry(const String* path, struct Stream* stream, struct ZipState* s) {
	String name = *path; 
	Utils_UNSAFE_GetFilename(&name);
	Event_RaiseEntry(&TextureEvents.FileChanged, stream, &name);
	return 0;
}

/* Extracts all the files from a stream representing a .zip archive */
static cc_result TexturePack_ExtractZip(struct Stream* stream) {
	struct ZipState state;
	Event_RaiseVoid(&TextureEvents.PackChanged);
	if (Gfx.LostContext) return 0;
	
	Zip_Init(&state, stream);
	state.ProcessEntry = TexturePack_ProcessZipEntry;
	return Zip_Extract(&state);
}

/* Changes the current terrain atlas from a stream representing a .png image */
/* Raises TextureEvents.PackChanged, so behaves as a .zip with only terrain.png in it */
static cc_result TexturePack_ExtractPng(struct Stream* stream) {
	Bitmap bmp; 
	cc_result res = Png_Decode(&bmp, stream);

	if (!res) {
		Event_RaiseVoid(&TextureEvents.PackChanged);
		if (Atlas_TryChange(&bmp)) return 0;
	}

	Mem_Free(bmp.scan0);
	return res;
}

/* Extracts a .zip texture pack from the given file */
static void TexturePack_ExtractZip_File(const String* filename) {
	String path; char pathBuffer[FILENAME_SIZE];
	struct Stream stream;
	cc_result res;

	/* TODO: This is an ugly hack to load textures from memory. */
	/* We need to mount /classicube to IndexedDB, but texpacks folder */
	/* should be read from memory. I don't know how to do that, */
	/* since mounting /classicube/texpacks to memory doesn't work.. */
#ifdef CC_BUILD_WEB
	extern int chdir(const char* path);
	chdir("/");
#endif

	String_InitArray(path, pathBuffer);
	String_Format1(&path, "texpacks/%s", filename);

	res = Stream_OpenFile(&stream, &path);
	if (res) { Logger_Warn2(res, "opening", &path); return; }

	res = TexturePack_ExtractZip(&stream);
	if (res) { Logger_Warn2(res, "extracting", &path); }

	res = stream.Close(&stream);
	if (res) { Logger_Warn2(res, "closing", &path); }

#ifdef CC_BUILD_WEB
	Platform_SetDefaultCurrentDirectory(0, NULL);
#endif
}

void TexturePack_ExtractInitial(void) {
	String texPack;
	Options_Get(OPT_DEFAULT_TEX_PACK, &defTexPack, "default.zip");
	TexturePack_ExtractZip_File(&defaultZip);

	/* in case the user's default texture pack doesn't have all required textures */
	texPack = TexturePack_UNSAFE_GetDefault();
	if (!String_CaselessEqualsConst(&texPack, "default.zip")) {
		TexturePack_ExtractZip_File(&texPack);
	}
}

static cc_bool texturePackDefault = true;
void TexturePack_ExtractCurrent(cc_bool forceReload) {
	String url = World_TextureUrl, file;
	struct Stream stream;
	cc_bool zip;
	cc_result res;

	if (!url.length || !OpenCachedData(&url, &stream)) {
		/* don't pointlessly load default texture pack */
		if (texturePackDefault && !forceReload) return;
		file = TexturePack_UNSAFE_GetDefault();

		TexturePack_ExtractZip_File(&file);
		texturePackDefault = true;
	} else {
		zip = String_ContainsConst(&url, ".zip");
		res = zip ? TexturePack_ExtractZip(&stream) : TexturePack_ExtractPng(&stream);
		if (res) Logger_Warn2(res, zip ? "extracting" : "decoding", &url);

		res = stream.Close(&stream);
		if (res) Logger_Warn2(res, "closing cache for", &url);
		texturePackDefault = false;
	}
}

void TexturePack_Apply(struct HttpRequest* item) {
	String url;
	cc_uint8* data; cc_uint32 len;
	struct Stream mem;
	cc_bool png;
	cc_result res;

	url = String_FromRawArray(item->url);
	UpdateCache(item);
	/* Took too long to download and is no longer active texture pack */
	if (!String_Equals(&World_TextureUrl, &url)) return;

	data = item->data;
	len  = item->size;
	Stream_ReadonlyMemory(&mem, data, len);

	png = Png_Detect(data, len);
	res = png ? TexturePack_ExtractPng(&mem) : TexturePack_ExtractZip(&mem);

	if (res) Logger_Warn2(res, png ? "decoding" : "extracting", &url);
	texturePackDefault = false;
}

void TexturePack_DownloadAsync(const String* url, const String* id) {
	String etag = String_Empty;
	String time = String_Empty;

	/* Only retrieve etag/last-modified headers if the file exists */
	/* This inconsistency can occur if user deleted some cached files */
	if (IsCached(url)) {
		time = GetCachedLastModified(url);
		etag = GetCachedETag(url);
	}
	Http_AsyncGetDataEx(url, true, id, &time, &etag, NULL);
}


/*########################################################################################################################*
*---------------------------------------------------Textures component----------------------------------------------------*
*#########################################################################################################################*/
static void OnFileChanged(void* obj, struct Stream* stream, const String* name) {
	Bitmap bmp;
	cc_result res;

	if (!String_CaselessEqualsConst(name, "terrain.png")) return;
	res = Png_Decode(&bmp, stream);

	if (res) {
		Logger_Warn2(res, "decoding", name);
		Mem_Free(bmp.scan0);
	} else if (!Atlas_TryChange(&bmp)) {
		Mem_Free(bmp.scan0);
	}
}

static void OnContextLost(void* obj) {
	if (!Gfx.ManagedTextures) Atlas1D_Free();
}

static void OnContextRecreated(void* obj) {
	if (!Gfx.ManagedTextures) TexturePack_ExtractCurrent(true);
}

static void Textures_Init(void) {
	Event_RegisterEntry(&TextureEvents.FileChanged, NULL, OnFileChanged);
	Event_RegisterVoid(&GfxEvents.ContextLost,      NULL, OnContextLost);
	Event_RegisterVoid(&GfxEvents.ContextRecreated, NULL, OnContextRecreated);

	TextureCache_Init();
}

static void Textures_Free(void) {
	Event_UnregisterEntry(&TextureEvents.FileChanged, NULL, OnFileChanged);
	Event_UnregisterVoid(&GfxEvents.ContextLost,      NULL, OnContextLost);
	Event_UnregisterVoid(&GfxEvents.ContextRecreated, NULL, OnContextRecreated);

	OnContextLost(NULL);
	Atlas2D_Free();
}

struct IGameComponent Textures_Component = {
	Textures_Init, /* Init  */
	Textures_Free  /* Free  */
};
