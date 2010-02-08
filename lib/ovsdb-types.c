/* Copyright (c) 2009, 2010 Nicira Networks
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>

#include "ovsdb-types.h"

#include <float.h>
#include <limits.h>

#include "dynamic-string.h"
#include "json.h"
#include "ovsdb-error.h"
#include "ovsdb-parser.h"

const struct ovsdb_type ovsdb_type_integer =
    OVSDB_TYPE_SCALAR_INITIALIZER(OVSDB_BASE_INTEGER_INIT);
const struct ovsdb_type ovsdb_type_real =
    OVSDB_TYPE_SCALAR_INITIALIZER(OVSDB_BASE_REAL_INIT);
const struct ovsdb_type ovsdb_type_boolean =
    OVSDB_TYPE_SCALAR_INITIALIZER(OVSDB_BASE_BOOLEAN_INIT);
const struct ovsdb_type ovsdb_type_string =
    OVSDB_TYPE_SCALAR_INITIALIZER(OVSDB_BASE_STRING_INIT);
const struct ovsdb_type ovsdb_type_uuid =
    OVSDB_TYPE_SCALAR_INITIALIZER(OVSDB_BASE_UUID_INIT);

/* ovsdb_atomic_type */
const char *
ovsdb_atomic_type_to_string(enum ovsdb_atomic_type type)
{
    switch (type) {
    case OVSDB_TYPE_VOID:
        return "void";

    case OVSDB_TYPE_INTEGER:
        return "integer";

    case OVSDB_TYPE_REAL:
        return "real";

    case OVSDB_TYPE_BOOLEAN:
        return "boolean";

    case OVSDB_TYPE_STRING:
        return "string";

    case OVSDB_TYPE_UUID:
        return "uuid";

    case OVSDB_N_TYPES:
    default:
        return "<invalid>";
    }
}

struct json *
ovsdb_atomic_type_to_json(enum ovsdb_atomic_type type)
{
    return json_string_create(ovsdb_atomic_type_to_string(type));
}

bool
ovsdb_atomic_type_from_string(const char *string, enum ovsdb_atomic_type *type)
{
    if (!strcmp(string, "integer")) {
        *type = OVSDB_TYPE_INTEGER;
    } else if (!strcmp(string, "real")) {
        *type = OVSDB_TYPE_REAL;
    } else if (!strcmp(string, "boolean")) {
        *type = OVSDB_TYPE_BOOLEAN;
    } else if (!strcmp(string, "string")) {
        *type = OVSDB_TYPE_STRING;
    } else if (!strcmp(string, "uuid")) {
        *type = OVSDB_TYPE_UUID;
    } else {
        return false;
    }
    return true;
}

struct ovsdb_error *
ovsdb_atomic_type_from_json(enum ovsdb_atomic_type *type,
                            const struct json *json)
{
    if (json->type == JSON_STRING) {
        if (ovsdb_atomic_type_from_string(json_string(json), type)) {
            return NULL;
        } else {
            *type = OVSDB_TYPE_VOID;
            return ovsdb_syntax_error(json, NULL,
                                      "\"%s\" is not an atomic-type",
                                      json_string(json));
        }
    } else {
        *type = OVSDB_TYPE_VOID;
        return ovsdb_syntax_error(json, NULL, "atomic-type expected");
    }
}

/* ovsdb_base_type */

void
ovsdb_base_type_init(struct ovsdb_base_type *base, enum ovsdb_atomic_type type)
{
    base->type = type;

    switch (base->type) {
    case OVSDB_TYPE_VOID:
        break;

    case OVSDB_TYPE_INTEGER:
        base->u.integer.min = INT64_MIN;
        base->u.integer.max = INT64_MAX;
        break;

    case OVSDB_TYPE_REAL:
        base->u.real.min = -DBL_MAX;
        base->u.real.max = DBL_MAX;
        break;

    case OVSDB_TYPE_BOOLEAN:
        break;

    case OVSDB_TYPE_STRING:
        base->u.string.re = NULL;
        base->u.string.reMatch = NULL;
        base->u.string.reComment = NULL;
        base->u.string.minLen = 0;
        base->u.string.maxLen = UINT_MAX;
        break;

    case OVSDB_TYPE_UUID:
        break;

    case OVSDB_N_TYPES:
        NOT_REACHED();

    default:
        NOT_REACHED();
    }
}

void
ovsdb_base_type_clone(struct ovsdb_base_type *dst,
                      const struct ovsdb_base_type *src)
{
    *dst = *src;

    switch (dst->type) {
    case OVSDB_TYPE_VOID:
    case OVSDB_TYPE_INTEGER:
    case OVSDB_TYPE_REAL:
    case OVSDB_TYPE_BOOLEAN:
        break;

    case OVSDB_TYPE_STRING:
        if (dst->u.string.re) {
            pcre_refcount(dst->u.string.re, 1);
        }
        break;

    case OVSDB_TYPE_UUID:
        break;

    case OVSDB_N_TYPES:
    default:
        NOT_REACHED();
    }
}

void
ovsdb_base_type_destroy(struct ovsdb_base_type *base)
{
    if (base) {
        switch (base->type) {
        case OVSDB_TYPE_VOID:
        case OVSDB_TYPE_INTEGER:
        case OVSDB_TYPE_REAL:
        case OVSDB_TYPE_BOOLEAN:
            break;

        case OVSDB_TYPE_STRING:
            if (base->u.string.re && !pcre_refcount(base->u.string.re, -1)) {
                pcre_free(base->u.string.re);
                free(base->u.string.reMatch);
                free(base->u.string.reComment);
            }
            break;

        case OVSDB_TYPE_UUID:
            break;

        case OVSDB_N_TYPES:
            NOT_REACHED();

        default:
            NOT_REACHED();
        }
    }
}

bool
ovsdb_base_type_is_valid(const struct ovsdb_base_type *base)
{
    switch (base->type) {
    case OVSDB_TYPE_VOID:
        return true;

    case OVSDB_TYPE_INTEGER:
        return base->u.integer.min <= base->u.integer.max;

    case OVSDB_TYPE_REAL:
        return base->u.real.min <= base->u.real.max;

    case OVSDB_TYPE_BOOLEAN:
        return true;

    case OVSDB_TYPE_STRING:
        return base->u.string.minLen <= base->u.string.maxLen;

    case OVSDB_TYPE_UUID:
        return true;

    case OVSDB_N_TYPES:
    default:
        return false;
    }
}

bool
ovsdb_base_type_has_constraints(const struct ovsdb_base_type *base)
{
    switch (base->type) {
    case OVSDB_TYPE_VOID:
        NOT_REACHED();

    case OVSDB_TYPE_INTEGER:
        return (base->u.integer.min != INT64_MIN
                || base->u.integer.max != INT64_MAX);

    case OVSDB_TYPE_REAL:
        return (base->u.real.min != -DBL_MAX
                || base->u.real.max != DBL_MAX);

    case OVSDB_TYPE_BOOLEAN:
        return false;

    case OVSDB_TYPE_STRING:
        return (base->u.string.reMatch != NULL
                || base->u.string.minLen != 0
                || base->u.string.maxLen != UINT_MAX);

    case OVSDB_TYPE_UUID:
        return false;

    case OVSDB_N_TYPES:
        NOT_REACHED();

    default:
        NOT_REACHED();
    }
}

void
ovsdb_base_type_clear_constraints(struct ovsdb_base_type *base)
{
    enum ovsdb_atomic_type type = base->type;
    ovsdb_base_type_destroy(base);
    ovsdb_base_type_init(base, type);
}

struct ovsdb_error *
ovsdb_base_type_set_regex(struct ovsdb_base_type *base,
                          const char *reMatch, const char *reComment)
{
    const char *errorString;
    const char *pattern;
    int errorOffset;

    /* Compile pattern, anchoring it at both ends. */
    pattern = reMatch;
    if (pattern[0] == '\0' || strchr(pattern, '\0')[-1] != '$') {
        pattern = xasprintf("%s$", pattern);
    }
    base->u.string.re = pcre_compile(pattern, (PCRE_ANCHORED | PCRE_UTF8
                                               | PCRE_JAVASCRIPT_COMPAT),
                                     &errorString, &errorOffset, NULL);
    if (pattern != reMatch) {
        free((char *) pattern);
    }
    if (!base->u.string.re) {
        return ovsdb_syntax_error(NULL, "invalid regular expression",
                                  "\"%s\" is not a valid regular "
                                  "expression: %s", reMatch, errorString);
    }

    /* Save regular expression. */
    pcre_refcount(base->u.string.re, 1);
    base->u.string.reMatch = xstrdup(reMatch);
    base->u.string.reComment = reComment ? xstrdup(reComment) : NULL;
    return NULL;
}

static struct ovsdb_error *
parse_optional_uint(struct ovsdb_parser *parser, const char *member,
                    unsigned int *uint)
{
    const struct json *json;

    json = ovsdb_parser_member(parser, member, OP_INTEGER | OP_OPTIONAL);
    if (json) {
        if (json->u.integer < 0 || json->u.integer > UINT_MAX) {
            return ovsdb_syntax_error(json, NULL,
                                      "%s out of valid range 0 to %u",
                                      member, UINT_MAX);
        }
        *uint = json->u.integer;
    }
    return NULL;
}

struct ovsdb_error *
ovsdb_base_type_from_json(struct ovsdb_base_type *base,
                          const struct json *json)
{
    struct ovsdb_parser parser;
    struct ovsdb_error *error;
    const struct json *type;

    if (json->type == JSON_STRING) {
        error = ovsdb_atomic_type_from_json(&base->type, json);
        if (error) {
            return error;
        }
        ovsdb_base_type_init(base, base->type);
        return NULL;
    }

    ovsdb_parser_init(&parser, json, "ovsdb type");
    type = ovsdb_parser_member(&parser, "type", OP_STRING);
    if (ovsdb_parser_has_error(&parser)) {
        base->type = OVSDB_TYPE_VOID;
        return ovsdb_parser_finish(&parser);
    }

    error = ovsdb_atomic_type_from_json(&base->type, type);
    if (error) {
        return error;
    }

    ovsdb_base_type_init(base, base->type);
    if (base->type == OVSDB_TYPE_INTEGER) {
        const struct json *min, *max;

        min = ovsdb_parser_member(&parser, "minInteger",
                                  OP_INTEGER | OP_OPTIONAL);
        max = ovsdb_parser_member(&parser, "maxInteger",
                                  OP_INTEGER | OP_OPTIONAL);
        base->u.integer.min = min ? min->u.integer : INT64_MIN;
        base->u.integer.max = max ? max->u.integer : INT64_MAX;
        if (base->u.integer.min > base->u.integer.max) {
            error = ovsdb_syntax_error(json, NULL,
                                       "minInteger exceeds maxInteger");
        }
    } else if (base->type == OVSDB_TYPE_REAL) {
        const struct json *min, *max;

        min = ovsdb_parser_member(&parser, "minReal", OP_NUMBER | OP_OPTIONAL);
        max = ovsdb_parser_member(&parser, "maxReal", OP_NUMBER | OP_OPTIONAL);
        base->u.real.min = min ? json_real(min) : -DBL_MAX;
        base->u.real.max = max ? json_real(max) : DBL_MAX;
        if (base->u.real.min > base->u.real.max) {
            error = ovsdb_syntax_error(json, NULL, "minReal exceeds maxReal");
        }
    } else if (base->type == OVSDB_TYPE_STRING) {
        const struct json *reMatch;

        reMatch = ovsdb_parser_member(&parser, "reMatch",
                                      OP_STRING | OP_OPTIONAL);
        if (reMatch) {
            const struct json *reComment;

            reComment = ovsdb_parser_member(&parser, "reComment",
                                            OP_STRING | OP_OPTIONAL);
            error = ovsdb_base_type_set_regex(
                base, json_string(reMatch),
                reComment ? json_string(reComment) : NULL);
        }

        if (!error) {
            error = parse_optional_uint(&parser, "minLength",
                                        &base->u.string.minLen);
        }
        if (!error) {
            error = parse_optional_uint(&parser, "maxLength",
                                        &base->u.string.maxLen);
        }
        if (!error && base->u.string.minLen > base->u.string.maxLen) {
            error = ovsdb_syntax_error(json, NULL,
                                       "minLength exceeds maxLength");
        }
    }

    if (error) {
        ovsdb_error_destroy(ovsdb_parser_finish(&parser));
    } else {
        error = ovsdb_parser_finish(&parser);
    }
    if (error) {
        ovsdb_base_type_destroy(base);
        base->type = OVSDB_TYPE_VOID;
    }
    return error;
}

struct json *
ovsdb_base_type_to_json(const struct ovsdb_base_type *base)
{
    struct json *json;

    if (!ovsdb_base_type_has_constraints(base)) {
        return json_string_create(ovsdb_atomic_type_to_string(base->type));
    }

    json = json_object_create();
    json_object_put_string(json, "type",
                           ovsdb_atomic_type_to_string(base->type));
    switch (base->type) {
    case OVSDB_TYPE_VOID:
        NOT_REACHED();

    case OVSDB_TYPE_INTEGER:
        if (base->u.integer.min != INT64_MIN) {
            json_object_put(json, "minInteger",
                            json_integer_create(base->u.integer.min));
        }
        if (base->u.integer.max != INT64_MAX) {
            json_object_put(json, "maxInteger",
                            json_integer_create(base->u.integer.max));
        }
        break;

    case OVSDB_TYPE_REAL:
        if (base->u.real.min != -DBL_MAX) {
            json_object_put(json, "minReal",
                            json_real_create(base->u.real.min));
        }
        if (base->u.real.max != DBL_MAX) {
            json_object_put(json, "maxReal",
                            json_real_create(base->u.real.max));
        }
        break;

    case OVSDB_TYPE_BOOLEAN:
        break;

    case OVSDB_TYPE_STRING:
        if (base->u.string.reMatch) {
            json_object_put_string(json, "reMatch", base->u.string.reMatch);
            if (base->u.string.reComment) {
                json_object_put_string(json, "reComment",
                                       base->u.string.reComment);
            }
        }
        if (base->u.string.minLen != 0) {
            json_object_put(json, "minLength",
                            json_integer_create(base->u.string.minLen));
        }
        if (base->u.string.maxLen != UINT_MAX) {
            json_object_put(json, "maxLength",
                            json_integer_create(base->u.string.maxLen));
        }
        break;

    case OVSDB_TYPE_UUID:
        break;

    case OVSDB_N_TYPES:
        NOT_REACHED();

    default:
        NOT_REACHED();
    }

    return json;
}

/* ovsdb_type */

void
ovsdb_type_clone(struct ovsdb_type *dst, const struct ovsdb_type *src)
{
    ovsdb_base_type_clone(&dst->key, &src->key);
    ovsdb_base_type_clone(&dst->value, &src->value);
    dst->n_min = src->n_min;
    dst->n_max = src->n_max;
}

void
ovsdb_type_destroy(struct ovsdb_type *type)
{
    ovsdb_base_type_destroy(&type->key);
    ovsdb_base_type_destroy(&type->value);
}

bool
ovsdb_type_is_valid(const struct ovsdb_type *type)
{
    return (type->key.type != OVSDB_TYPE_VOID
            && ovsdb_base_type_is_valid(&type->key)
            && ovsdb_base_type_is_valid(&type->value)
            && type->n_min <= 1
            && type->n_min <= type->n_max);
}

static struct ovsdb_error *
n_from_json(const struct json *json, unsigned int *n)
{
    if (!json) {
        return NULL;
    } else if (json->type == JSON_INTEGER
               && json->u.integer >= 0 && json->u.integer < UINT_MAX) {
        *n = json->u.integer;
        return NULL;
    } else {
        return ovsdb_syntax_error(json, NULL, "bad min or max value");
    }
}

char *
ovsdb_type_to_english(const struct ovsdb_type *type)
{
    const char *key = ovsdb_atomic_type_to_string(type->key.type);
    const char *value = ovsdb_atomic_type_to_string(type->value.type);
    if (ovsdb_type_is_scalar(type)) {
        return xstrdup(key);
    } else {
        struct ds s = DS_EMPTY_INITIALIZER;
        ds_put_cstr(&s, ovsdb_type_is_set(type) ? "set" : "map");
        if (type->n_max == UINT_MAX) {
            if (type->n_min) {
                ds_put_format(&s, " of %u or more", type->n_min);
            } else {
                ds_put_cstr(&s, " of");
            }
        } else if (type->n_min) {
            ds_put_format(&s, " of %u to %u", type->n_min, type->n_max);
        } else {
            ds_put_format(&s, " of up to %u", type->n_max);
        }
        if (ovsdb_type_is_set(type)) {
            ds_put_format(&s, " %ss", key);
        } else {
            ds_put_format(&s, " (%s, %s) pairs", key, value);
        }
        return ds_cstr(&s);
    }
}

struct ovsdb_error *
ovsdb_type_from_json(struct ovsdb_type *type, const struct json *json)
{
    type->value.type = OVSDB_TYPE_VOID;
    type->n_min = 1;
    type->n_max = 1;

    if (json->type == JSON_STRING) {
        return ovsdb_base_type_from_json(&type->key, json);
    } else if (json->type == JSON_OBJECT) {
        const struct json *key, *value, *min, *max;
        struct ovsdb_error *error;
        struct ovsdb_parser parser;

        ovsdb_parser_init(&parser, json, "ovsdb type");
        key = ovsdb_parser_member(&parser, "key", OP_STRING | OP_OBJECT);
        value = ovsdb_parser_member(&parser, "value",
                                    OP_STRING | OP_OBJECT | OP_OPTIONAL);
        min = ovsdb_parser_member(&parser, "min", OP_INTEGER | OP_OPTIONAL);
        max = ovsdb_parser_member(&parser, "max",
                                  OP_INTEGER | OP_STRING | OP_OPTIONAL);
        error = ovsdb_parser_finish(&parser);
        if (error) {
            return error;
        }

        error = ovsdb_base_type_from_json(&type->key, key);
        if (error) {
            return error;
        }

        if (value) {
            error = ovsdb_base_type_from_json(&type->value, value);
            if (error) {
                return error;
            }
        }

        error = n_from_json(min, &type->n_min);
        if (error) {
            return error;
        }

        if (max && max->type == JSON_STRING
            && !strcmp(max->u.string, "unlimited")) {
            type->n_max = UINT_MAX;
        } else {
            error = n_from_json(max, &type->n_max);
            if (error) {
                return error;
            }
        }

        if (!ovsdb_type_is_valid(type)) {
            return ovsdb_syntax_error(json, NULL,
                                      "ovsdb type fails constraint checks");
        }

        return NULL;
    } else {
        return ovsdb_syntax_error(json, NULL, "ovsdb type expected");
    }
}

struct json *
ovsdb_type_to_json(const struct ovsdb_type *type)
{
    if (ovsdb_type_is_scalar(type)
        && !ovsdb_base_type_has_constraints(&type->key)) {
        return ovsdb_base_type_to_json(&type->key);
    } else {
        struct json *json = json_object_create();
        json_object_put(json, "key", ovsdb_base_type_to_json(&type->key));
        if (type->value.type != OVSDB_TYPE_VOID) {
            json_object_put(json, "value",
                            ovsdb_base_type_to_json(&type->value));
        }
        if (type->n_min != 1) {
            json_object_put(json, "min", json_integer_create(type->n_min));
        }
        if (type->n_max == UINT_MAX) {
            json_object_put_string(json, "max", "unlimited");
        } else if (type->n_max != 1) {
            json_object_put(json, "max", json_integer_create(type->n_max));
        }
        return json;
    }
}
