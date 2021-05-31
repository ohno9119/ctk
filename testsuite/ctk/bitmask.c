/* CtkRBTree tests.
 *
 * Copyright (C) 2011, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <locale.h>

#include "../../ctk/ctkbitmaskprivate.h"

#include <string.h>

/* how often we run the random tests */
#define N_RUNS 20

/* how many tries we do in our random tests */
#define N_TRIES 100

/* the maximum index we use for bitmask values */
#define MAX_INDEX 1000

/* UTILITIES */

static CtkBitmask *
ctk_bitmask_new_parse (const char *string)
{
  guint i, length;
  CtkBitmask *mask;

  length = strlen (string);
  mask = _ctk_bitmask_new ();

  for (i = 0; i < length; i++)
    {
      if (string[i] == '0')
        mask = _ctk_bitmask_set (mask, length - i - 1, FALSE);
      else if (string[i] == '1')
        mask = _ctk_bitmask_set (mask, length - i - 1, TRUE);
      else
        g_assert_not_reached ();
    }

  return mask;
}

#define assert_cmpmasks(mask,other) G_STMT_START { \
  if (G_UNLIKELY (!_ctk_bitmask_equals (mask, other))) \
    { \
      char *mask_string = _ctk_bitmask_to_string (mask); \
      char *other_string = _ctk_bitmask_to_string (other); \
      char *msg = g_strdup_printf ("%s (%s) != %s (%s)", \
                                   G_STRINGIFY (mask), mask_string, \
                                   G_STRINGIFY (other), other_string); \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, msg); \
      g_free (msg); \
      g_free (mask_string); \
      g_free (other_string); \
    } \
}G_STMT_END

static const char *tests[] = {
                                                                                                                                   "0",
                                                                                                                                   "1",
                                                                                                     "1000000000000000000000000000000",
                                                                                                    "10000000000000000000000000000000",
                                                                     "100000000000000000000000000000000000000000000000000000000000000",
                                                                    "1000000000000000000000000000000000000000000000000000000000000000",
                                                                   "10000000000000000000000000000000000000000000000000000000000000000",
  "1010101010101010101010101010101010101010101010101010101010101010101010101010101010101010101010101010101010101010101010101010101010",
  "1000010000100001000010000100001000010000100001000010000100001000010000100001000010000100001000010000100001000010000100001000010000",
  "1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
};

static CtkBitmask *masks[G_N_ELEMENTS (tests)];

/* TEST */

static void
test_to_string (void)
{
  guint i;
  char *to_string;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      to_string = _ctk_bitmask_to_string (masks[i]);
      g_assert_cmpstr (to_string, ==, tests[i]);
      g_free (to_string);
    }
}

static void
test_is_empty (void)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_assert_cmpint (_ctk_bitmask_is_empty (masks[i]), ==, i == 0);
    }
}

static void
test_equals (void)
{
  guint i, j;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      for (j = 0; j < G_N_ELEMENTS (tests); j++)
        {
          g_assert_cmpint (_ctk_bitmask_equals (masks[i], masks[j]), ==, i == j);
        }
    }
}

static void
test_set (void)
{
  guint i, j;
  guint indexes[N_TRIES];
  CtkBitmask *copy;
  const CtkBitmask *mask;

  for (i = 0; i < N_RUNS; i++)
    {
      mask = masks[g_test_rand_int_range (0, G_N_ELEMENTS (tests))];
      copy = _ctk_bitmask_copy (mask);

      for (j = 0; j < N_TRIES; j++)
        {
          indexes[j] = g_test_rand_int_range (0, MAX_INDEX);
          copy = _ctk_bitmask_set (copy, indexes[j], g_test_rand_bit ());
        }

      for (j = 0; j < N_TRIES; j++)
        {
          copy = _ctk_bitmask_set (copy, indexes[j], _ctk_bitmask_get (mask, indexes[j]));
        }

      assert_cmpmasks (copy, mask);
      _ctk_bitmask_free (copy);
    }
}

static void
test_union (void)
{
  CtkBitmask *left, *right, *expected;
  guint run, try, n_tries;

  for (run = 0; run < N_RUNS; run++)
    {
      left = _ctk_bitmask_new ();
      right = _ctk_bitmask_new ();
      expected = _ctk_bitmask_new ();

      n_tries = g_test_perf () ? N_TRIES : g_test_rand_int_range (0, N_TRIES);
      for (try = 0; try < n_tries; try++)
        {
          guint id = g_test_rand_int_range (0, MAX_INDEX);

          if (g_test_rand_bit ())
            left = _ctk_bitmask_set (left, id, TRUE);
          else
            right = _ctk_bitmask_set (right, id, TRUE);

          expected = _ctk_bitmask_set (expected, id, TRUE);
        }

      left = _ctk_bitmask_union (left, right);
      right = _ctk_bitmask_union (right, left);

      assert_cmpmasks (left, expected);
      assert_cmpmasks (right, expected);
      _ctk_bitmask_free (left);
      _ctk_bitmask_free (right);
      _ctk_bitmask_free (expected);
    }
}

static void
test_intersect (void)
{
  CtkBitmask *left, *right, *expected;
  guint run, try;
  gboolean intersects;

  for (run = 0; run < N_RUNS; run++)
    {
      left = _ctk_bitmask_new ();
      right = _ctk_bitmask_new ();
      expected = _ctk_bitmask_new ();

      for (try = 0; try < N_TRIES; try++)
        {
          guint id = g_test_rand_int_range (0, MAX_INDEX);
          gboolean set = g_test_rand_bit ();

          if (g_test_rand_bit ())
            {
              left = _ctk_bitmask_set (left, id, set);
              expected = _ctk_bitmask_set (expected, id, set ? _ctk_bitmask_get (right, id) : 0);
            }
          else
            {
              right = _ctk_bitmask_set (right, id, set);
              expected = _ctk_bitmask_set (expected, id, set ? _ctk_bitmask_get (left, id) : 0);
            }
        }

      intersects = _ctk_bitmask_intersects (left, right);
      g_assert_cmpint (intersects, ==, _ctk_bitmask_intersects (right, left));
      g_assert_cmpint (intersects, !=, _ctk_bitmask_is_empty (expected));

      left = _ctk_bitmask_intersect (left, right);
      right = _ctk_bitmask_intersect (right, left);

      assert_cmpmasks (left, expected);
      assert_cmpmasks (right, expected);
      _ctk_bitmask_free (left);
      _ctk_bitmask_free (right);
      _ctk_bitmask_free (expected);
    }
}

static void
test_intersect_hardcoded (void)
{
  CtkBitmask *left, *right, *intersection, *expected;
  const char *left_str, *right_str;
  guint left_len, right_len;
  guint i, l, r;

  for (l = 0; l < G_N_ELEMENTS (tests); l++)
    {
      for (r = 0; r < G_N_ELEMENTS (tests); r++)
        {
          left = masks[l];
          right = masks[r];
          left_str = tests[l];
          right_str = tests[r];
          left_len = strlen (tests[l]);
          right_len = strlen (tests[r]);

          expected = _ctk_bitmask_new ();
          if (left_len > right_len)
            left_str += left_len - right_len;
          if (right_len > left_len)
            right_str += right_len - left_len;
          i = MIN (right_len, left_len);
          while (i--)
            {
              expected = _ctk_bitmask_set (expected, i, left_str[0] == '1' && right_str[0] == '1');
              right_str++;
              left_str++;
            }

          intersection = _ctk_bitmask_intersect (_ctk_bitmask_copy (left), right);

          assert_cmpmasks (intersection, expected);
          g_assert_cmpint (_ctk_bitmask_is_empty (expected), ==, !_ctk_bitmask_intersects (left, right));

          _ctk_bitmask_free (intersection);
          _ctk_bitmask_free (expected);
        }
    }
}

static void
test_subtract_hardcoded (void)
{
  CtkBitmask *left, *right, *subtracted, *expected;
  const char *left_str, *right_str;
  guint left_len, right_len;
  guint i, l, r;

  for (l = 0; l < G_N_ELEMENTS (tests); l++)
    {
      for (r = 0; r < G_N_ELEMENTS (tests); r++)
        {
          left = masks[l];
          right = masks[r];
          left_str = tests[l];
          right_str = tests[r];
          left_len = strlen (tests[l]);
          right_len = strlen (tests[r]);

          expected = _ctk_bitmask_new ();
          for (i = MIN (right_len, left_len); i < left_len; i++)
            {
              expected = _ctk_bitmask_set (expected, i, left_str[left_len - i - 1] == '1');
            }
          if (left_len > right_len)
            left_str += left_len - right_len;
          if (right_len > left_len)
            right_str += right_len - left_len;
          i = MIN (right_len, left_len);
          while (i--)
            {
              expected = _ctk_bitmask_set (expected, i, left_str[0] == '1' && right_str[0] == '0');
              right_str++;
              left_str++;
            }

          g_test_message ("%s - %s\n", _ctk_bitmask_to_string (left), _ctk_bitmask_to_string (right));
          subtracted = _ctk_bitmask_subtract (_ctk_bitmask_copy (left), right);

          assert_cmpmasks (subtracted, expected);

          _ctk_bitmask_free (subtracted);
          _ctk_bitmask_free (expected);
        }
    }
}

#define SWAP(_a, _b) G_STMT_START{ \
  guint _tmp = _a; \
  _a = _b; \
  _b = _tmp; \
}G_STMT_END

static void
test_invert_range_hardcoded (void)
{
  guint t, l, r, i;
  gsize r_len, l_len, ref_len;
  char *ref_str;
  CtkBitmask *bitmask, *ref;

  for (t = 0; t < G_N_ELEMENTS (tests); t++)
    {
      for (l = 0; l < G_N_ELEMENTS (tests); l++)
        {
          l_len = strlen (tests[l]);

          for (r = 0; r < G_N_ELEMENTS (tests); r++)
            {
              r_len = strlen (tests[r]);
              if (r_len < l_len)
                continue;
              
              ref_len = MAX (r_len, strlen (tests[t]));
              ref_str = g_strdup_printf ("%*s", (int) ref_len, tests[t]);
              for (i = 0; i < ref_len && ref_str[i] == ' '; i++)
                ref_str[i] = '0';
              for (i = l_len - 1; i < r_len; i++)
                {
                  ref_str[ref_len-i-1] = ref_str[ref_len-i-1] == '0' ? '1' : '0';
                }
              ref = ctk_bitmask_new_parse (ref_str);
              g_free (ref_str);

              bitmask = ctk_bitmask_new_parse (tests[t]);
              bitmask = _ctk_bitmask_invert_range (bitmask, l_len - 1, r_len);

              assert_cmpmasks (bitmask, ref);

              _ctk_bitmask_free (bitmask);
              _ctk_bitmask_free (ref);
            }
        }
    }
}

static void
test_invert_range (void)
{
  CtkBitmask *left, *right, *intersection, *expected;
  guint run;
  guint left_start, left_end, right_start, right_end, start, end;

  for (run = 0; run < N_RUNS; run++)
    {
      left = _ctk_bitmask_new ();
      right = _ctk_bitmask_new ();
      expected = _ctk_bitmask_new ();

      left_start = g_test_rand_int_range (0, MAX_INDEX);
      left_end = g_test_rand_int_range (0, MAX_INDEX);
      if (left_start > left_end)
        SWAP (left_start, left_end);
      right_start = g_test_rand_int_range (0, MAX_INDEX);
      right_end = g_test_rand_int_range (0, MAX_INDEX);
      if (right_start > right_end)
        SWAP (right_start, right_end);
      start = MAX (left_start, right_start);
      end = MIN (left_end, right_end);

      if (left_start != left_end)
        left = _ctk_bitmask_invert_range (left, left_start, left_end);
      if (right_start != right_end)
        right = _ctk_bitmask_invert_range (right, right_start, right_end);
      if (start < end)
        expected = _ctk_bitmask_invert_range (expected, start, end);

      intersection = _ctk_bitmask_copy (left);
      intersection = _ctk_bitmask_intersect (intersection, right);

      assert_cmpmasks (intersection, expected);

      if (start < end)
        expected = _ctk_bitmask_invert_range (expected, start, end);

      g_assert_cmpint (_ctk_bitmask_is_empty (expected), ==, TRUE);

      _ctk_bitmask_free (left);
      _ctk_bitmask_free (right);
      _ctk_bitmask_free (intersection);
      _ctk_bitmask_free (expected);
    }
}

/* SETUP & RUNNING */

static void
create_masks (void)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    masks[i] = ctk_bitmask_new_parse (tests[i]);
}

static void
free_masks (void)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      _ctk_bitmask_free (masks[i]);
      masks[i] = NULL;
    }
}

int
main (int argc, char *argv[])
{
  int result;

  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  create_masks ();

  g_test_add_func ("/bitmask/to_string", test_to_string);
  g_test_add_func ("/bitmask/is_empty", test_is_empty);
  g_test_add_func ("/bitmask/equals", test_equals);
  g_test_add_func ("/bitmask/set", test_set);
  g_test_add_func ("/bitmask/union", test_union);
  g_test_add_func ("/bitmask/intersect", test_intersect);
  g_test_add_func ("/bitmask/intersect_hardcoded", test_intersect_hardcoded);
  g_test_add_func ("/bitmask/subtract_hardcoded", test_subtract_hardcoded);
  g_test_add_func ("/bitmask/invert_range", test_invert_range);
  g_test_add_func ("/bitmask/invert_range_hardcoded", test_invert_range_hardcoded);

  result = g_test_run ();

  free_masks ();

  return result;
}
