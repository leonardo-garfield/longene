/*
 * msvcrt.dll wide-char functions
 *
 * Copyright 1999 Alexandre Julliard
 * Copyright 2000 Jon Griffiths
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "msvcrt.h"
#include "winnls.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);


/*********************************************************************
 *		_wcsdup (MSVCRT.@)
 */
MSVCRT_wchar_t* CDECL _wcsdup( const MSVCRT_wchar_t* str )
{
  MSVCRT_wchar_t* ret = NULL;
  if (str)
  {
    int size = (strlenW(str) + 1) * sizeof(MSVCRT_wchar_t);
    ret = MSVCRT_malloc( size );
    if (ret) memcpy( ret, str, size );
  }
  return ret;
}

/*********************************************************************
 *		_wcsicoll (MSVCRT.@)
 */
INT CDECL _wcsicoll( const MSVCRT_wchar_t* str1, const MSVCRT_wchar_t* str2 )
{
  /* FIXME: handle collates */
  return strcmpiW( str1, str2 );
}

/*********************************************************************
 *		_wcsnset (MSVCRT.@)
 */
MSVCRT_wchar_t* CDECL _wcsnset( MSVCRT_wchar_t* str, MSVCRT_wchar_t c, MSVCRT_size_t n )
{
  MSVCRT_wchar_t* ret = str;
  while ((n-- > 0) && *str) *str++ = c;
  return ret;
}

/*********************************************************************
 *		_wcsrev (MSVCRT.@)
 */
MSVCRT_wchar_t* CDECL _wcsrev( MSVCRT_wchar_t* str )
{
  MSVCRT_wchar_t* ret = str;
  MSVCRT_wchar_t* end = str + strlenW(str) - 1;
  while (end > str)
  {
    MSVCRT_wchar_t t = *end;
    *end--  = *str;
    *str++  = t;
  }
  return ret;
}

/*********************************************************************
 *		_wcsset (MSVCRT.@)
 */
MSVCRT_wchar_t* CDECL _wcsset( MSVCRT_wchar_t* str, MSVCRT_wchar_t c )
{
  MSVCRT_wchar_t* ret = str;
  while (*str) *str++ = c;
  return ret;
}

/*********************************************************************
 *		wcstod (MSVCRT.@)
 */
double CDECL MSVCRT_wcstod(const MSVCRT_wchar_t* lpszStr, MSVCRT_wchar_t** end)
{
  const MSVCRT_wchar_t* str = lpszStr;
  int negative = 0;
  double ret = 0, divisor = 10.0;

  TRACE("(%s,%p) semi-stub\n", debugstr_w(lpszStr), end);

  /* FIXME:
   * - Should set errno on failure
   * - Should fail on overflow
   * - Need to check which input formats are allowed
   */
  while (isspaceW(*str))
    str++;

  if (*str == '-')
  {
    negative = 1;
    str++;
  }

  while (isdigitW(*str))
  {
    ret = ret * 10.0 + (*str - '0');
    str++;
  }
  if (*str == '.')
    str++;
  while (isdigitW(*str))
  {
    ret = ret + (*str - '0') / divisor;
    divisor *= 10;
    str++;
  }

  if (*str == 'E' || *str == 'e' || *str == 'D' || *str == 'd')
  {
    int negativeExponent = 0;
    int exponent = 0;
    if (*(++str) == '-')
    {
      negativeExponent = 1;
      str++;
    }
    while (isdigitW(*str))
    {
      exponent = exponent * 10 + (*str - '0');
      str++;
    }
    if (exponent != 0)
    {
      if (negativeExponent)
        ret = ret / pow(10.0, exponent);
      else
        ret = ret * pow(10.0, exponent);
    }
  }

  if (negative)
    ret = -ret;

  if (end)
    *end = (MSVCRT_wchar_t*)str;

  TRACE("returning %g\n", ret);
  return ret;
}


typedef struct pf_output_t
{
    int used;
    int len;
    BOOL unicode;
    union {
        LPWSTR W;
        LPSTR  A;
    } buf;
} pf_output;

typedef struct pf_flags_t
{
    char Sign, LeftAlign, Alternate, PadZero;
    int FieldLength, Precision;
    char IntegerLength, IntegerDouble;
    char WideString;
    char Format;
} pf_flags;

/*
 * writes a string of characters to the output
 * returns -1 if the string doesn't fit in the output buffer
 * return the length of the string if all characters were written
 */
static inline int pf_output_stringW( pf_output *out, LPCWSTR str, int len )
{
    int space = out->len - out->used;

    if( len < 0 )
        len = strlenW( str );
    if( out->unicode )
    {
        LPWSTR p = out->buf.W + out->used;

        if( space >= len )
        {
            memcpy( p, str, len*sizeof(WCHAR) );
            out->used += len;
            return len;
        }
        if( space > 0 )
            memcpy( p, str, space*sizeof(WCHAR) );
        out->used += len;
    }
    else
    {
        int n = WideCharToMultiByte( CP_ACP, 0, str, len, NULL, 0, NULL, NULL );
        LPSTR p = out->buf.A + out->used;

        if( space >= n )
        {
            WideCharToMultiByte( CP_ACP, 0, str, len, p, n, NULL, NULL );
            out->used += n;
            return len;
        }
        if( space > 0 )
            WideCharToMultiByte( CP_ACP, 0, str, len, p, space, NULL, NULL );
        out->used += n;
    }
    return -1;
}

static inline int pf_output_stringA( pf_output *out, LPCSTR str, int len )
{
    int space = out->len - out->used;

    if( len < 0 )
        len = strlen( str );
    if( !out->unicode )
    {
        LPSTR p = out->buf.A + out->used;

        if( space >= len )
        {
            memcpy( p, str, len );
            out->used += len;
            return len;
        }
        if( space > 0 )
            memcpy( p, str, space );
        out->used += len;
    }
    else
    {
        int n = MultiByteToWideChar( CP_ACP, 0, str, len, NULL, 0 );
        LPWSTR p = out->buf.W + out->used;

        if( space >= n )
        {
            MultiByteToWideChar( CP_ACP, 0, str, len, p, n );
            out->used += n;
            return len;
        }
        if( space > 0 )
            MultiByteToWideChar( CP_ACP, 0, str, len, p, space );
        out->used += n;
    }
    return -1;
}

/* pf_fill: takes care of signs, alignment, zero and field padding */
static inline int pf_fill( pf_output *out, int len, pf_flags *flags, char left )
{
    int i, r = 0;

    if( flags->Sign && !( flags->Format == 'd' || flags->Format == 'i' ) )
        flags->Sign = 0;

    if( left && flags->Sign )
    {
        flags->FieldLength--;
        if( flags->PadZero )
            r = pf_output_stringA( out, &flags->Sign, 1 );
    }

    if( ( !left &&  flags->LeftAlign ) || 
        (  left && !flags->LeftAlign ))
    {
        for( i=0; (i<(flags->FieldLength-len)) && (r>=0); i++ )
        {
            if( left && flags->PadZero )
                r = pf_output_stringA( out, "0", 1 );
            else
                r = pf_output_stringA( out, " ", 1 );
        }
    }

    if( left && flags->Sign && !flags->PadZero )
        r = pf_output_stringA( out, &flags->Sign, 1 );

    return r;
}

static inline int pf_output_format_W( pf_output *out, LPCWSTR str,
                                      int len, pf_flags *flags )
{
    int r = 0;

    if( len < 0 )
        len = strlenW( str );

    if (flags->Precision >= 0 && flags->Precision < len)
        len = flags->Precision;

    r = pf_fill( out, len, flags, 1 );

    if( r>=0 )
        r = pf_output_stringW( out, str, len );

    if( r>=0 )
        r = pf_fill( out, len, flags, 0 );

    return r;
}

static inline int pf_output_format_A( pf_output *out, LPCSTR str,
                                      int len, pf_flags *flags )
{
    int r = 0;

    if( len < 0 )
        len = strlen( str );

    if (flags->Precision >= 0 && flags->Precision < len)
        len = flags->Precision;

    r = pf_fill( out, len, flags, 1 );

    if( r>=0 )
        r = pf_output_stringA( out, str, len );

    if( r>=0 )
        r = pf_fill( out, len, flags, 0 );

    return r;
}

static int pf_handle_string_format( pf_output *out, const void* str, int len,
                             pf_flags *flags, BOOL capital_letter)
{
     if(str == NULL)  /* catch NULL pointer */
        return pf_output_format_A( out, "(null)", -1, flags);

     /* prefixes take priority over %c,%s vs. %C,%S, so we handle them first */
    if(flags->WideString || flags->IntegerLength == 'l')
        return pf_output_format_W( out, str, len, flags);
    if(flags->IntegerLength == 'h')
        return pf_output_format_A( out, str, len, flags);

    /* %s,%c ->  chars in ansi functions & wchars in unicode
     * %S,%C -> wchars in ansi functions &  chars in unicode */
    if( capital_letter == out->unicode) /* either both TRUE or both FALSE */
        return pf_output_format_A( out, str, len, flags);
    else
        return pf_output_format_W( out, str, len, flags);
}

static inline BOOL pf_is_integer_format( char fmt )
{
    static const char float_fmts[] = "diouxX";
    if (!fmt)
        return FALSE;
    return strchr( float_fmts, fmt ) ? TRUE : FALSE;
}

static inline BOOL pf_is_double_format( char fmt )
{
    static const char float_fmts[] = "aeEfgG";
    if (!fmt)
        return FALSE;
    return strchr( float_fmts, fmt ) ? TRUE : FALSE;
}

static inline BOOL pf_is_valid_format( char fmt )
{
    static const char float_fmts[] = "acCdeEfgGinouxX";
    if (!fmt)
        return FALSE;
    return strchr( float_fmts, fmt ) ? TRUE : FALSE;
}

static void pf_rebuild_format_string( char *p, pf_flags *flags )
{
    *p++ = '%';
    if( flags->Sign )
        *p++ = flags->Sign;
    if( flags->LeftAlign )
        *p++ = flags->LeftAlign;
    if( flags->Alternate )
        *p++ = flags->Alternate;
    if( flags->PadZero )
        *p++ = flags->PadZero;
    if( flags->FieldLength )
    {
        sprintf(p, "%d", flags->FieldLength);
        p += strlen(p);
    }
    if( flags->Precision >= 0 )
    {
        sprintf(p, ".%d", flags->Precision);
        p += strlen(p);
    }
    *p++ = flags->Format;
    *p++ = 0;
}

/* pf_integer_conv:  prints x to buf, including alternate formats and
   additional precision digits, but not field characters or the sign */
static void pf_integer_conv( char *buf, int buf_len, pf_flags *flags,
                             LONGLONG x )
{
    unsigned int base;
    const char *digits;

    int i, j, k;
    char number[40], *tmp = number;

    if( buf_len > sizeof number )
        tmp = HeapAlloc( GetProcessHeap(), 0, buf_len );

    base = 10;
    if( flags->Format == 'o' )
        base = 8;
    else if( flags->Format == 'x' || flags->Format == 'X' )
        base = 16;

    if( flags->Format == 'X' )
        digits = "0123456789ABCDEFX";
    else
        digits = "0123456789abcdefx";

    if( x < 0 && ( flags->Format == 'd' || flags->Format == 'i' ) )
    {
        x = -x;
        flags->Sign = '-';
    }

    /* Do conversion (backwards) */
    i = 0;
    if( x == 0 && flags->Precision )
        tmp[i++] = '0';
    else
        while( x != 0 )
        {
            j = (ULONGLONG) x % base;
            x = (ULONGLONG) x / base;
            tmp[i++] = digits[j];
        }
    k = flags->Precision - i;
    while( k-- > 0 )
        tmp[i++] = '0';
    if( flags->Alternate )
    {
        if( base == 16 )
        {
            tmp[i++] = digits[16];
            tmp[i++] = '0';
        }
        else if( base == 8 && tmp[i-1] != '0' )
            tmp[i++] = '0';
    }

    /* Reverse for buf */
    j = 0;
    while( i-- > 0 )
        buf[j++] = tmp[i];
    buf[j] = '\0';

    /* Adjust precision so pf_fill won't truncate the number later */
    flags->Precision = strlen( buf );

    if( tmp != number )
        HeapFree( GetProcessHeap(), 0, tmp );

    return;
}

/*********************************************************************
 *  pf_vsnprintf  (INTERNAL)
 *
 *  implements both A and W vsnprintf functions
 */
static int pf_vsnprintf( pf_output *out, const WCHAR *format, va_list valist )
{
    int r;
    LPCWSTR q, p = format;
    pf_flags flags;

    TRACE("format is %s\n",debugstr_w(format));
    while (*p)
    {
        q = strchrW( p, '%' );

        /* there's no % characters left, output the rest of the string */
        if( !q )
        {
            r = pf_output_stringW(out, p, -1);
            if( r<0 )
                return r;
            p += r;
            continue;
        }

        /* there's characters before the %, output them */
        if( q != p )
        {
            r = pf_output_stringW(out, p, q - p);
            if( r<0 )
                return r;
            p = q;
        }

        /* we must be at a % now, skip over it */
        assert( *p == '%' );
        p++;

        /* output a single % character */
        if( *p == '%' )
        {
            r = pf_output_stringW(out, p++, 1);
            if( r<0 )
                return r;
            continue;
        }

        /* parse the flags */
        memset( &flags, 0, sizeof flags );
        while (*p)
        {
            if( *p == '+' || *p == ' ' )
            {
                if ( flags.Sign != '+' )
                    flags.Sign = *p;
            }
            else if( *p == '-' )
                flags.LeftAlign = *p;
            else if( *p == '0' )
                flags.PadZero = *p;
            else if( *p == '#' )
                flags.Alternate = *p;
            else
                break;
            p++;
        }

        /* deal with the field width specifier */
        flags.FieldLength = 0;
        if( *p == '*' )
        {
            flags.FieldLength = va_arg( valist, int );
            if (flags.FieldLength < 0)
            {
                flags.LeftAlign = '-';
                flags.FieldLength = -flags.FieldLength;
            }
            p++;
        }
        else while( isdigit(*p) )
        {
            flags.FieldLength *= 10;
            flags.FieldLength += *p++ - '0';
        }

        /* deal with precision */
        flags.Precision = -1;
        if( *p == '.' )
        {
            flags.Precision = 0;
            p++;
            if( *p == '*' )
            {
                flags.Precision = va_arg( valist, int );
                p++;
            }
            else while( isdigit(*p) )
            {
                flags.Precision *= 10;
                flags.Precision += *p++ - '0';
            }
        }

        /* deal with integer width modifier */
        while( *p )
        {
            if( *p == 'h' || *p == 'l' || *p == 'L' )
            {
                flags.IntegerLength = *p;
                p++;
            }
            else if( *p == 'I' )
            {
                if( *(p+1) == '6' && *(p+2) == '4' )
                {
                    flags.IntegerDouble++;
                    p += 3;
                }
                else if( *(p+1) == '3' && *(p+2) == '2' )
                    p += 3;
                else if( isdigit(*(p+1)) || *(p+1) == 0 )
                    break;
                else
                    p++;
            }
            else if( *p == 'w' )
                flags.WideString = *p++;
            else if( *p == 'F' )
                p++; /* ignore */
            else
                break;
        }

        flags.Format = *p;
        r = 0;

        /* output a string */
        if(  flags.Format == 's' || flags.Format == 'S' )
            r = pf_handle_string_format( out, va_arg(valist, const void*), -1,
                                         &flags, (flags.Format == 'S') );

        /* output a single character */
        else if( flags.Format == 'c' || flags.Format == 'C' )
        {
            INT ch = va_arg( valist, int );

            r = pf_handle_string_format( out, &ch, 1, &flags, (flags.Format == 'C') );
        }

        /* output a pointer */
        else if( flags.Format == 'p' )
        {
            char pointer[11];

            flags.PadZero = 0;
            if( flags.Alternate )
                sprintf(pointer, "0X%08lX", va_arg(valist, long));
            else
                sprintf(pointer, "%08lX", va_arg(valist, long));
            r = pf_output_format_A( out, pointer, -1, &flags );
        }

        /* deal with %n */
        else if( flags.Format == 'n' )
        {
            int *x = va_arg(valist, int *);
            *x = out->used;
        }

        /* deal with 64-bit integers */
        else if( pf_is_integer_format( flags.Format ) && flags.IntegerDouble )
        {
            char number[40], *x = number;

            /* Estimate largest possible required buffer size:
               * Chooses the larger of the field or precision
               * Includes extra bytes: 1 byte for null, 1 byte for sign,
                 4 bytes for exponent, 2 bytes for alternate formats, 1 byte 
                 for a decimal, and 1 byte for an additional float digit. */
            int x_len = ((flags.FieldLength > flags.Precision) ? 
                        flags.FieldLength : flags.Precision) + 10;

            if( x_len >= sizeof number)
                x = HeapAlloc( GetProcessHeap(), 0, x_len );

            pf_integer_conv( x, x_len, &flags, va_arg(valist, LONGLONG) );

            r = pf_output_format_A( out, x, -1, &flags );
            if( x != number )
                HeapFree( GetProcessHeap(), 0, x );
        }

        /* deal with integers and floats using libc's printf */
        else if( pf_is_valid_format( flags.Format ) )
        {
            char fmt[20], number[40], *x = number;

            /* Estimate largest possible required buffer size:
               * Chooses the larger of the field or precision
               * Includes extra bytes: 1 byte for null, 1 byte for sign,
                 4 bytes for exponent, 2 bytes for alternate formats, 1 byte 
                 for a decimal, and 1 byte for an additional float digit. */
            int x_len = ((flags.FieldLength > flags.Precision) ? 
                        flags.FieldLength : flags.Precision) + 10;

            if( x_len >= sizeof number)
                x = HeapAlloc( GetProcessHeap(), 0, x_len );

            pf_rebuild_format_string( fmt, &flags );

            if( pf_is_double_format( flags.Format ) )
                sprintf( x, fmt, va_arg(valist, double) );
            else
                sprintf( x, fmt, va_arg(valist, int) );

            r = pf_output_stringA( out, x, -1 );
            if( x != number )
                HeapFree( GetProcessHeap(), 0, x );
        }
        else
            continue;

        if( r<0 )
            return r;
        p++;
    }

    /* check we reached the end, and null terminate the string */
    assert( *p == 0 );
    pf_output_stringW( out, p, 1 );

    return out->used - 1;
}

/*********************************************************************
 *		_vsnprintf (MSVCRT.@)
 */
int CDECL MSVCRT_vsnprintf( char *str, unsigned int len,
                            const char *format, va_list valist )
{
    DWORD sz;
    LPWSTR formatW = NULL;
    pf_output out;
    int r;

    out.unicode = FALSE;
    out.buf.A = str;
    out.used = 0;
    out.len = len;

    if( format )
    {
        sz = MultiByteToWideChar( CP_ACP, 0, format, -1, NULL, 0 );
        formatW = HeapAlloc( GetProcessHeap(), 0, sz*sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, format, -1, formatW, sz );
    }

    r = pf_vsnprintf( &out, formatW, valist );

    HeapFree( GetProcessHeap(), 0, formatW );

    return r;
}

/*********************************************************************
 *		vsprintf (MSVCRT.@)
 */
int CDECL MSVCRT_vsprintf( char *str, const char *format, va_list valist)
{
    return MSVCRT_vsnprintf(str, INT_MAX, format, valist);
}

/*********************************************************************
 *		_snprintf (MSVCRT.@)
 */
int CDECL MSVCRT__snprintf(char *str, unsigned int len, const char *format, ...)
{
    int retval;
    va_list valist;
    va_start(valist, format);
    retval = MSVCRT_vsnprintf(str, len, format, valist);
    va_end(valist);
    return retval;
}

/*********************************************************************
 *		_vsnwsprintf (MSVCRT.@)
 */
int CDECL MSVCRT_vsnwprintf( MSVCRT_wchar_t *str, unsigned int len,
                             const MSVCRT_wchar_t *format, va_list valist )
{
    pf_output out;

    out.unicode = TRUE;
    out.buf.W = str;
    out.used = 0;
    out.len = len;

    return pf_vsnprintf( &out, format, valist );
}

/*********************************************************************
 *		_snwprintf (MSVCRT.@)
 */
int CDECL MSVCRT__snwprintf( MSVCRT_wchar_t *str, unsigned int len, const MSVCRT_wchar_t *format, ...)
{
    int retval;
    va_list valist;
    va_start(valist, format);
    retval = MSVCRT_vsnwprintf(str, len, format, valist);
    va_end(valist);
    return retval;
}

/*********************************************************************
 *		sprintf (MSVCRT.@)
 */
int CDECL MSVCRT_sprintf( char *str, const char *format, ... )
{
    va_list ap;
    int r;

    va_start( ap, format );
    r = MSVCRT_vsnprintf( str, INT_MAX, format, ap );
    va_end( ap );
    return r;
}

/*********************************************************************
 *		swprintf (MSVCRT.@)
 */
int CDECL MSVCRT_swprintf( MSVCRT_wchar_t *str, const MSVCRT_wchar_t *format, ... )
{
    va_list ap;
    int r;

    va_start( ap, format );
    r = MSVCRT_vsnwprintf( str, INT_MAX, format, ap );
    va_end( ap );
    return r;
}

/*********************************************************************
 *		vswprintf (MSVCRT.@)
 */
int CDECL MSVCRT_vswprintf( MSVCRT_wchar_t* str, const MSVCRT_wchar_t* format, va_list args )
{
    return MSVCRT_vsnwprintf( str, INT_MAX, format, args );
}

/*********************************************************************
 *		wcscoll (MSVCRT.@)
 */
int CDECL MSVCRT_wcscoll( const MSVCRT_wchar_t* str1, const MSVCRT_wchar_t* str2 )
{
  /* FIXME: handle collates */
  return strcmpW( str1, str2 );
}

/*********************************************************************
 *		wcspbrk (MSVCRT.@)
 */
MSVCRT_wchar_t* CDECL MSVCRT_wcspbrk( const MSVCRT_wchar_t* str, const MSVCRT_wchar_t* accept )
{
  const MSVCRT_wchar_t* p;
  while (*str)
  {
    for (p = accept; *p; p++) if (*p == *str) return (MSVCRT_wchar_t*)str;
      str++;
  }
  return NULL;
}

/*********************************************************************
 *		wcstok  (MSVCRT.@)
 */
MSVCRT_wchar_t * CDECL MSVCRT_wcstok( MSVCRT_wchar_t *str, const MSVCRT_wchar_t *delim )
{
    thread_data_t *data = msvcrt_get_thread_data();
    MSVCRT_wchar_t *ret;

    if (!str)
        if (!(str = data->wcstok_next)) return NULL;

    while (*str && strchrW( delim, *str )) str++;
    if (!*str) return NULL;
    ret = str++;
    while (*str && !strchrW( delim, *str )) str++;
    if (*str) *str++ = 0;
    data->wcstok_next = str;
    return ret;
}


/*********************************************************************
 *		wctomb (MSVCRT.@)
 */
INT CDECL MSVCRT_wctomb( char *dst, MSVCRT_wchar_t ch )
{
  return WideCharToMultiByte( CP_ACP, 0, &ch, 1, dst, 6, NULL, NULL );
}

/*********************************************************************
 *		iswalnum (MSVCRT.@)
 */
INT CDECL MSVCRT_iswalnum( MSVCRT_wchar_t wc )
{
    return isalnumW( wc );
}

/*********************************************************************
 *		iswalpha (MSVCRT.@)
 */
INT CDECL MSVCRT_iswalpha( MSVCRT_wchar_t wc )
{
    return isalphaW( wc );
}

/*********************************************************************
 *		iswcntrl (MSVCRT.@)
 */
INT CDECL MSVCRT_iswcntrl( MSVCRT_wchar_t wc )
{
    return iscntrlW( wc );
}

/*********************************************************************
 *		iswdigit (MSVCRT.@)
 */
INT CDECL MSVCRT_iswdigit( MSVCRT_wchar_t wc )
{
    return isdigitW( wc );
}

/*********************************************************************
 *		iswgraph (MSVCRT.@)
 */
INT CDECL MSVCRT_iswgraph( MSVCRT_wchar_t wc )
{
    return isgraphW( wc );
}

/*********************************************************************
 *		iswlower (MSVCRT.@)
 */
INT CDECL MSVCRT_iswlower( MSVCRT_wchar_t wc )
{
    return islowerW( wc );
}

/*********************************************************************
 *		iswprint (MSVCRT.@)
 */
INT CDECL MSVCRT_iswprint( MSVCRT_wchar_t wc )
{
    return isprintW( wc );
}

/*********************************************************************
 *		iswpunct (MSVCRT.@)
 */
INT CDECL MSVCRT_iswpunct( MSVCRT_wchar_t wc )
{
    return ispunctW( wc );
}

/*********************************************************************
 *		iswspace (MSVCRT.@)
 */
INT CDECL MSVCRT_iswspace( MSVCRT_wchar_t wc )
{
    return isspaceW( wc );
}

/*********************************************************************
 *		iswupper (MSVCRT.@)
 */
INT CDECL MSVCRT_iswupper( MSVCRT_wchar_t wc )
{
    return isupperW( wc );
}

/*********************************************************************
 *		iswxdigit (MSVCRT.@)
 */
INT CDECL MSVCRT_iswxdigit( MSVCRT_wchar_t wc )
{
    return isxdigitW( wc );
}

/*********************************************************************
 *		wcscpy_s (MSVCRT.@)
 */
INT CDECL MSVCRT_wcscpy_s( MSVCRT_wchar_t* wcDest, MSVCRT_size_t numElement, const  MSVCRT_wchar_t *wcSrc)
{
    INT size = 0;

    if(!wcDest || !numElement)
        return MSVCRT_EINVAL;

    wcDest[0] = 0;

    if(!wcSrc)
    {
        return MSVCRT_EINVAL;
    }

    size = strlenW(wcSrc) + 1;

    if(size > numElement)
    {
        return MSVCRT_ERANGE;
    }

    if(size > numElement)
        size = numElement;

    memcpy( wcDest, wcSrc, size*sizeof(WCHAR) );

    return 0;
}
