/* alloc.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dlfcn.h>
#include <glib.h>
#include <glib-object.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct _AllocTest AllocTest;
typedef void (*AllocTestFunc) (const AllocTest *test);

struct _AllocTest
{
   unsigned n_iterations;
   unsigned size;
   AllocTestFunc test_func;
};

static void
alloc_test_impl_gslice (const AllocTest *test)
{
   unsigned i;
   void *ptr;

   for (i = 0; i < test->n_iterations; i++) {
      ptr = g_slice_alloc (test->size);
      g_assert (ptr);
      g_slice_free1 (test->size, ptr);
   }
}

static void
alloc_test_impl_gobject (const AllocTest *test)
{
   unsigned i;
   void *ptr;

   for (i = 0; i < test->n_iterations; i++) {
      ptr = g_object_new (G_TYPE_OBJECT, NULL);
      g_assert (ptr);
      g_object_unref (ptr);
   }
}

static void
alloc_test_impl_malloc (const AllocTest *test)
{
   unsigned i;
   void *ptr;

   for (i = 0; i < test->n_iterations; i++) {
      ptr = malloc (test->size);
      g_assert (ptr);
      free (ptr);
   }
}

static void
alloc_test_impl_gmalloc (const AllocTest *test)
{
   unsigned i;
   void *ptr;

   for (i = 0; i < test->n_iterations; i++) {
      ptr = g_malloc (test->size);
      g_assert (ptr);
      g_free (ptr);
   }
}

static void
usage (FILE *stream)
{
   fprintf (stream,
            "  malloc        Test default allocation and release.\n"
            "  gslice        Test gslice allocation and release.\n"
            "  gmalloc       Test gmalloc allocation and release.\n"
            "  gobject       Test GObject allocation and release.\n"
            "\n");
}

int
main (int argc,
      char *argv[])
{
   AllocTest test = { 0 };
   gint64 begin;
   gint64 end;
   gint64 total_usec;
   gint64 total_sec;
   gdouble total_time;
   gint iter = 1000000;
   gint size = 128;
   gchar *command = NULL;
   GOptionEntry entries [] = {
      { "iterations", 'i', 0, G_OPTION_ARG_INT, &iter, "The number of iterations to perform.", "1000000" },
      { "size", 's', 0, G_OPTION_ARG_INT, &size, "The size of allocation to perform in bytes.", "128" },
      { "command", 'c', 0, G_OPTION_ARG_STRING, &command, "The command to run. Use 'list' to list available commands.", "NAME" },
      { NULL }
   };
   GOptionContext *context;
   GError *error = NULL;

   context = g_option_context_new ("- malloc performance tests.");
   g_option_context_add_main_entries (context, entries, NULL);

   if (!g_option_context_parse (context, &argc, &argv, &error)) {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      g_option_context_free (context);
      return EXIT_FAILURE;
   }

   if (iter < 1) {
      fprintf (stderr, "--iterations is too small.\n");
      return EXIT_FAILURE;
   } else if (size < 1) {
      fprintf (stderr, "--size is too small.\n");
      return EXIT_FAILURE;
   }

   test.n_iterations = iter;
   test.size = size;

   if (0 == g_strcmp0 (command, "list")) {
      usage (stdout);
      return EXIT_SUCCESS;
   } else if (0 == g_strcmp0 (command, "malloc")) {
      test.test_func = alloc_test_impl_malloc;
   } else if (0 == g_strcmp0 (command, "gslice")) {
      test.test_func = alloc_test_impl_gslice;
   } else if (0 == g_strcmp0 (command, "gobject")) {
      test.test_func = alloc_test_impl_gobject;
   } else if (0 == g_strcmp0 (command, "gmalloc")) {
      test.test_func = alloc_test_impl_gmalloc;
   } else {
      fprintf (stderr, "Please specify a valid command to run.\n"
                       "\n"
                       "Commands:\n");
      usage (stderr);
      return EXIT_FAILURE;
   }

   begin = g_get_monotonic_time ();
   test.test_func (&test);
   end = g_get_monotonic_time ();

   total_sec = (end - begin) / G_USEC_PER_SEC;
   total_usec = (end - begin) % G_USEC_PER_SEC;
   total_time = (gdouble)total_sec + (gdouble)total_usec / G_USEC_PER_SEC;

   fprintf (stdout, "%s %u %u %lf\n",
            command, iter, size, total_time);

   return EXIT_SUCCESS;
}
