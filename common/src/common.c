/** @file common.c
 *
 * @brief Common definitions source
 *
 * @par
 *
 */

#include "common.h"

status_t
display_hex (
    void       * p_buf,
    size_t       size,
    char const * p_sep,
    char const * p_end
)
{
    return fprint(
        stdout,
        p_buf,
        size,
        p_sep,
        p_end,
        NULL,
        "%02hhx",
        "%02hhx",
        true
    );
}

status_t
display_printable (
    void       * p_buf,
    size_t       size,
    char const * p_sep,
    char const * p_end
)
{
    return fprint(
        stdout,
        p_buf,
        size,
        p_sep,
        p_end,
        isprint,
        "%c",
        "",
        true
    );
}

status_t
display_unicode (
    void       * p_buf,
    size_t       size,
    char const * p_sep,
    char const * p_end
)
{
    return fprint(
        stdout,
        p_buf,
        size,
        p_sep,
        p_end,
        isprint,
        "%c",
        ".",
        true
    );
}

status_t
fprint (
    FILE              * p_stream,
    void              * p_buf,
    size_t              size,
    char const        * p_sep,
    char const        * p_end,
    ischartype_func_t   p_ischartype,
    char const        * p_fmt_true,
    char const        * p_fmt_false,
    bool                b_flush
)
{
    status_t status = STATUS_SUCCESS;

    char const * p_fmt = NULL;

    if (NULL == p_buf)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_stream     = (NULL == p_stream)     ? stdout      : p_stream;
    p_sep        = (NULL == p_sep)        ? " "         : p_sep;
    p_end        = (NULL == p_end)        ? "\n"        : p_end;
    p_ischartype = (NULL == p_ischartype) ? isprint     : p_ischartype;
    p_fmt_true   = (NULL == p_fmt_true)   ? "%c"        : p_fmt_true;
    p_fmt_false  = (NULL == p_fmt_false)  ? "\\x%02hhx" : p_fmt_false;

    for (size_t idx = 0u; idx < size; idx++)
    {
        uint8_t chr = ((uint8_t *)p_buf)[idx];

        // Print separator
        if (1u <= idx)
        {
            fprintf(p_stream, "%s", p_sep);
        }

        p_fmt = (0 != p_ischartype(chr)) ? p_fmt_true : p_fmt_false;

        // NOTE: fprintf expects string literal, ignore compiler warning
#       pragma GCC diagnostic push
#       pragma GCC diagnostic ignored "-Wformat-nonliteral"
        fprintf(p_stream, p_fmt, chr);
#       pragma GCC diagnostic pop
    }

    fprintf(p_stream, "%s", p_end);

    if (b_flush)
    {
        fflush(p_stream);
    }

cleanup:
    return status;
}

/*** end of file ***/
