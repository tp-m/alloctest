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
#include <malloc.h>
#include <pthread.h>
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
   unsigned active;
   AllocTestFunc test_func;
   pthread_mutex_t mutex;
   pthread_cond_t cond;
};

static void
alloc_test_impl_gslice (const AllocTest *test)
{
   gpointer *data;
   unsigned i;

   data = g_malloc_n (sizeof *data, test->n_iterations);

   for (i = 0; i < test->n_iterations; i++) {
      data [i] = g_slice_alloc (test->size);
      memset (data [i], 0, 4);
   }

   for (i = 0; i < test->n_iterations; i++) {
      g_slice_free1 (test->size, data [i]);
   }

   g_free (data);
}

static void
alloc_test_impl_gobject (const AllocTest *test)
{
   gpointer *data;
   unsigned i;

   data = g_malloc_n (sizeof *data, test->n_iterations);

   for (i = 0; i < test->n_iterations; i++) {
      data [i] = g_object_new (G_TYPE_OBJECT, NULL);
   }

   for (i = 0; i < test->n_iterations; i++) {
      g_object_unref (data [i]);
   }

   g_free (data);
}

static void
alloc_test_impl_malloc (const AllocTest *test)
{
   gpointer *data;
   unsigned i;

   data = g_malloc_n (sizeof *data, test->n_iterations);

   for (i = 0; i < test->n_iterations; i++) {
      data [i] = malloc (test->size);
      memset (data [i], 0, 4);
   }

   for (i = 0; i < test->n_iterations; i++) {
      free (data [i]);
   }

   g_free (data);
}

static void
alloc_test_impl_gmalloc (const AllocTest *test)
{
   gpointer *data;
   unsigned i;

   data = g_malloc_n (sizeof *data, test->n_iterations);

   for (i = 0; i < test->n_iterations; i++) {
      data [i] = g_malloc (test->size);
      memset (data [i], 0, 4);
   }

   for (i = 0; i < test->n_iterations; i++) {
      free (data [i]);
   }

   g_free (data);
}

static void *
worker (void *data)
{
   AllocTest *test = data;

   pthread_mutex_lock (&test->mutex);
   test->active++;
   pthread_cond_wait (&test->cond, &test->mutex);
   pthread_mutex_unlock (&test->mutex);

   test->test_func (test);

   return NULL;
}

static gsize
get_vmpeak (void)
{
#ifdef __linux__
   gchar *contents = NULL;
   gchar *filename;
   gchar *buf;
   gsize len = 0;
   gsize ret = 0;

   filename = g_strdup_printf ("/proc/%u/status", (unsigned)getpid ());

   if (g_file_get_contents (filename, &contents, &len, NULL)) {
      buf = strstr (contents, "VmPeak:");
      if (buf) {
         buf += strlen ("VmPeak:");
         while (*buf == ' ')
            buf++;
         ret = g_ascii_strtoll (buf, NULL, 10);
         if (strstr (buf, "kB")) {
            ret *= 1024;
         } else if (strstr (buf, "mB")) {
            ret *= 1024 * 1024;
         }
      }
      g_free (contents);
   }

   g_free (filename);

   return ret;
#else
   return 0;
#endif
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
   struct mallinfo minfo;
   AllocTest test = { 0 };
   gsize vmpeak = 0;
   gint64 begin;
   gint64 end;
   gint64 total_usec;
   gint64 total_sec;
   gdouble total_time;
   gint iter = 1000000;
   gint size = 128;
   gint nthread = 1;
   gchar *command = NULL;
   GOptionEntry entries [] = {
      { "iterations", 'i', 0, G_OPTION_ARG_INT, &iter, "The number of iterations to perform.", "1000000" },
      { "size", 's', 0, G_OPTION_ARG_INT, &size, "The size of allocation to perform in bytes.", "128" },
      { "command", 'c', 0, G_OPTION_ARG_STRING, &command, "The command to run. Use 'list' to list available commands.", "NAME" },
      { "thread", 't', 0, G_OPTION_ARG_INT, &nthread, "The number of threads to run.", "1" },
      { NULL }
   };
   GOptionContext *context;
   GError *error = NULL;
   pthread_t *threads;
   int i;

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

   pthread_mutex_init (&test.mutex, NULL);
   pthread_cond_init (&test.cond, NULL);

   threads = calloc (nthread, sizeof (pthread_t));

   for (i = 0; i < nthread; i++) {
      pthread_create (&threads [i], NULL, worker, &test);
   }

   for (;;) {
      pthread_mutex_lock (&test.mutex);
      if (test.active == nthread) {
         break;
      }
      pthread_mutex_unlock (&test.mutex);
   }

   begin = g_get_monotonic_time ();
   pthread_cond_broadcast (&test.cond);
   pthread_mutex_unlock (&test.mutex);
   for (i = 0; i < nthread; i++) {
      pthread_join (threads [i], NULL);
   }
   end = g_get_monotonic_time ();

   total_sec = (end - begin) / G_USEC_PER_SEC;
   total_usec = (end - begin) % G_USEC_PER_SEC;
   total_time = (gdouble)total_sec + (gdouble)total_usec / G_USEC_PER_SEC;

   minfo = mallinfo ();
   vmpeak = get_vmpeak ();

   fprintf (stdout, "%s%s %u %u %u %lf %u %"G_GSIZE_FORMAT"\n",
            command, getenv ("LD_PRELOAD") ? "+tcmalloc" : "",
            iter, size, nthread, total_time, minfo.uordblks,
            vmpeak);

   pthread_mutex_destroy (&test.mutex);
   pthread_cond_destroy (&test.cond);

   return EXIT_SUCCESS;
}
