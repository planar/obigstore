
#include <leveldb/db.h>
#include <leveldb/comparator.h>
#include "crc.h"

#if defined(OS_MACOSX)
  #include <machine/endian.h>
#elif defined(OS_SOLARIS)
  #include <sys/isa_defs.h>
  #ifdef _LITTLE_ENDIAN
    #define LITTLE_ENDIAN
  #else
    #define BIG_ENDIAN
  #endif
#else
  #include <endian.h>
#endif

#ifdef LITTLE_ENDIAN
#define IS_LITTLE_ENDIAN true
#else
#define IS_LITTLE_ENDIAN (__BYTE_ORDER == __LITTLE_ENDIAN)
#endif

extern "C" {


#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>

const char cMetadata_prefix = 0;
const char cDatum_prefix = 1;

static int64_t
decode_var_int(const char *p)
{
 int64_t n = 0;
 int shift = 0;

 do {
     n += (*p & 0x7F) << shift;
     shift += 7;
 } while (*p++ & 0x80);

 return n;
}

class OStoreComparator1 : public leveldb::Comparator {
    public:
    OStoreComparator1() {}

    virtual const char* Name() const {
        return "org.eigenclass/OStoreComparator1";
    }

    virtual int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const {
        size_t sa = a.size(), sb = b.size();

        if(!sa && sb) return -1;
        if(!sb && sa) return 1;
        if(!sa && !sb) return 0;

        // need some care here: cannot do a[0] - b[0] because they are signed
        // chars
        if(a[0] != b[0]) return a.compare(b);

        switch(a[0]) {
            case '1': {
                // first compare keyspaces
                int ksa = decode_var_int(a.data() + 1),
                    ksb = decode_var_int(b.data() + 1);
                // don't expect billions of keyspaces, no pb with overflow
                int kscmp = ksa - ksb;
                if(kscmp) return kscmp;

                // then compare table ids
                int kslen_a = (a[sa - 4] >> 3) & 0x7,
                    kslen_b = (b[sb - 4] >> 3) & 0x7,
                    tlen_a = a[sa - 4] & 0x7,
                    tlen_b = b[sb - 4] & 0x7;

                int ta = decode_var_int(a.data() + 1 + kslen_a),
                    tb = decode_var_int(b.data() + 1 + kslen_b);

                // both ta and tb should be small, overflow not an issue in
                // practice
                int tblcmp = ta - tb;
                if(tblcmp) return tblcmp;

                // then decode column and key lengths, then compare the
                // latter, then the former

                int last_a = a[sa - 3], last_b = b[sb - 3];
                int clen_len_a = last_a & 0x7,
                    clen_len_b = last_b & 0x7;
                int klen_len_a = (last_a >> 3) & 0x7,
                    klen_len_b = (last_b >> 3) & 0x7;

                int64_t klen_a, klen_b;

                klen_a = decode_var_int(a.data() + sa - 4 - clen_len_a - klen_len_a);
                klen_b = decode_var_int(b.data() + sb - 4 - clen_len_b - klen_len_b);

                leveldb::Slice key_a(a.data() + 1 + kslen_a + tlen_a, klen_a),
                               key_b(b.data() + 1 + kslen_b + tlen_b, klen_b);

                int keycmp = key_a.compare(key_b);
                if(keycmp) return keycmp;

                int64_t clen_a, clen_b;
                clen_a = decode_var_int(a.data() + sa - 4 - clen_len_a);
                clen_b = decode_var_int(b.data() + sb - 4 - clen_len_b);

                leveldb::Slice col_a(a.data() + 1 + kslen_a + tlen_a + klen_a, clen_a),
                               col_b(b.data() + 1 + kslen_b + tlen_b + klen_b, clen_b);

                int colcmp = col_a.compare(col_b);
                return colcmp;

                break;
            }
            default:
                // bytewise comparison for all others
                return a.compare(b);
        }

    }

    virtual void FindShortestSeparator(std::string *start,
                                       const leveldb::Slice& limit) const {
        return;
    }

    virtual void FindShortSuccessor(std::string *key) const {
        return;
    }
};

static const OStoreComparator1 comparator1;

CAMLprim const leveldb::Comparator*
ostore_custom_comparator() {
    return &comparator1;
}

CAMLprim value
ostore_apply_custom_comparator(value s1, value s2)
{
 CAMLparam2(s1, s2);
 leveldb::Slice d1(String_val(s1), string_length(s1)),
                d2(String_val(s2), string_length(s2));

 CAMLreturn(Val_int(comparator1.Compare(d1, d2)));
}

CAMLprim value
ostore_bytea_blit_int64_complement_le(value dst, value off, value n)
{
 int64_t v = Int64_val(n);
 v = v ^ -1;

#ifdef IS_LITTLE_ENDIAN
 // FIXME: non-aligned writes
 int64_t *p = (int64_t *)(&Byte_u(dst, Long_val(off)));
 *p = v;
#else
 char *d = &Byte_u(dst, Long_val(off));
 char *s = (char *)&v;

 d[0] = s[7]; d[1] = s[6]; d[2] = s[5]; d[3] = s[4];
 d[4] = s[3]; d[5] = s[2]; d[6] = s[1]; d[7] = s[0];
#endif

 return(Val_unit);
}

CAMLprim value
ostore_bytea_blit_int64_le(value dst, value off, value n)
{
 int64_t v = Int64_val(n);

#ifdef IS_LITTLE_ENDIAN
 // FIXME: non-aligned writes
 int64_t *p = (int64_t *)(&Byte_u(dst, Long_val(off)));
 *p = v;
#else
 char *d = &Byte_u(dst, Long_val(off));
 char *s = (char *)&v;

 d[0] = s[7]; d[1] = s[6]; d[2] = s[5]; d[3] = s[4];
 d[4] = s[3]; d[5] = s[2]; d[6] = s[1]; d[7] = s[0];
#endif

 return(Val_unit);
}


CAMLprim value
ostore_bytea_blit_int_as_i32_le(value dst, value off, value n)
{
 int32_t v = Long_val(n);

#ifdef IS_LITTLE_ENDIAN
 // FIXME: non-aligned writes
 int32_t *p = (int32_t *)(&Byte_u(dst, Long_val(off)));
 *p = v;
#else
 uint8_t *d = &Byte_u(dst, Long_val(off));
 uint8_t *s = (uint8_t *)&v;

 d[0] = s[3]; d[1] = s[2]; d[2] = s[1]; d[3] = s[0];
#endif

 return(Val_unit);
}

CAMLprim value
ostore_decode_int64_le(value s, value off)
{
#ifdef IS_LITTLE_ENDIAN
  int64_t p = *((int64_t *)(&Byte_u(s, Long_val(off)))) ^ -1;
  return(caml_copy_int64(p));
#else
  uint8_t *s = &Byte_u(s, Int_val(off));
  union {
      int64_t ll;
      unsigned char c[8];
  } x;

  x.c[0] = s[7]; x.c[1] = s[6]; x.c[2] = s[5]; x.c[3] = x[4];
  x.c[4] = s[3]; x.c[5] = s[2]; x.c[6] = s[1]; x.c[7] = x[0];

  return(caml_copy_int64(x.ll ^ -1));
#endif
}


CAMLprim value
ostore_decode_int64_complement_le(value s, value off)
{
#ifdef IS_LITTLE_ENDIAN
  int64_t p = *((int64_t *)(&Byte_u(s, Long_val(off)))) ^ -1;
  return(caml_copy_int64(p));
#else
  uint8_t *s = &Byte_u(s, Int_val(off));
  union {
      int64_t ll;
      unsigned char c[8];
  } x;

  x.c[0] = s[7]; x.c[1] = s[6]; x.c[2] = s[5]; x.c[3] = x[4];
  x.c[4] = s[3]; x.c[5] = s[2]; x.c[6] = s[1]; x.c[7] = x[0];

  return(caml_copy_int64(x.ll ^ -1));
#endif
}

static uint32_t
update_crc32c(uint32_t crc, const uint8_t *buf, size_t size)
{
 crc ^= 0xFFFFFFFF;
 while(size--) {
     crc = (crc >> 8) ^ crc32ctable[(crc ^ *buf++) & 0xFF];
 }
 crc ^= 0xFFFFFFFF;
 return crc;
}

CAMLprim value
ostore_crc32c_string(value s)
{
 CAMLparam1(s);
 CAMLlocal1(ret);

 ret = caml_alloc_string(4);
 uint32_t *p = (uint32_t *) String_val(ret);
 *p = update_crc32c(0, &Byte_u(s, 0), string_length(s));
 CAMLreturn(ret);
}

CAMLprim value
ostore_crc32c_update(value t, value s, value off, value len)
{
 uint32_t *p = (uint32_t *)String_val(t);
 *p = update_crc32c(*p, &Byte_u(s, Long_val(off)), Long_val(len));
 return Val_unit;
}

CAMLprim value
ostore_crc32c_ensure_lsb(value t)
{
 #ifndef IS_LITTLE_ENDIAN
 const uint8_t *p = &Byte_u(t, 0);
 uint8_t n;
 n = p[0]; p[0] = p[3]; p[3] = n;
 n = p[1]; p[1] = p[2]; p[2] = n;
 #endif

 return Val_unit;
}


}
