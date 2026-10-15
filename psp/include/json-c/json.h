/*
json-c/json.h - the bit of json-c the game uses (utils/json_config.c), on
top of cJSON, which the PSP toolchain ships.

"struct json_object" is cJSON's struct. Ownership follows json-c: an object
added to another belongs to it, json_object_put frees a root, and the string
from json_object_to_json_string_ext lives until the next call.
*/
#pragma once
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#define json_object cJSON

#define JSON_C_TO_STRING_PLAIN  0
#define JSON_C_TO_STRING_SPACED 1
#define JSON_C_TO_STRING_PRETTY 2

static inline struct json_object *json_object_new_object( void ) { return cJSON_CreateObject(); }
static inline struct json_object *json_object_new_string( const char *s ) { return cJSON_CreateString( s ? s : "" ); }
static inline struct json_object *json_object_new_int( int v ) { return cJSON_CreateNumber( v ); }
static inline struct json_object *json_object_new_double( double v ) { return cJSON_CreateNumber( v ); }
static inline struct json_object *json_object_new_boolean( int v ) { return cJSON_CreateBool( v ); }

static inline int json_object_put( struct json_object *obj )
{
	if( obj )
		cJSON_Delete( obj );
	return 1;
}

static inline int json_object_object_add( struct json_object *obj, const char *key, struct json_object *val )
{
	if( !obj || !key || !val )
		return -1;
	if( cJSON_GetObjectItemCaseSensitive( obj, key ))
		cJSON_ReplaceItemInObjectCaseSensitive( obj, key, val );
	else
		cJSON_AddItemToObject( obj, key, val );
	return 0;
}

static inline int json_object_object_get_ex( const struct json_object *obj, const char *key, struct json_object **value )
{
	struct json_object *v = obj && cJSON_IsObject( obj ) ? cJSON_GetObjectItemCaseSensitive( obj, key ) : NULL;
	if( value )
		*value = v;
	return v != NULL;
}

static inline struct json_object *json_tokener_parse( const char *str )
{
	return cJSON_Parse( str );
}

static inline const char *json_object_to_json_string_ext( struct json_object *obj, int flags )
{
	static char *last;
	if( last )
		cJSON_free( last );
	last = ( flags & JSON_C_TO_STRING_PRETTY ) ? cJSON_Print( obj ) : cJSON_PrintUnformatted( obj );
	return last ? last : "{}";
}

static inline const char *json_object_get_string( struct json_object *obj )
{
	if( obj && cJSON_IsString( obj ))
		return obj->valuestring;
	return "";
}

static inline int json_object_get_int( const struct json_object *obj )
{
	if( !obj ) return 0;
	if( cJSON_IsNumber( obj )) return obj->valueint;
	if( cJSON_IsTrue( obj )) return 1;
	if( cJSON_IsString( obj )) return atoi( obj->valuestring );
	return 0;
}

static inline double json_object_get_double( const struct json_object *obj )
{
	if( !obj ) return 0;
	if( cJSON_IsNumber( obj )) return obj->valuedouble;
	if( cJSON_IsTrue( obj )) return 1;
	if( cJSON_IsString( obj )) return atof( obj->valuestring );
	return 0;
}

static inline int json_object_get_boolean( const struct json_object *obj )
{
	if( !obj ) return 0;
	if( cJSON_IsBool( obj )) return cJSON_IsTrue( obj );
	if( cJSON_IsNumber( obj )) return obj->valuedouble != 0;
	if( cJSON_IsString( obj )) return obj->valuestring[0] != 0;
	return 0;
}
