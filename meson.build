project('alloctest', 'c')

gobject_dep = dependency('gobject-2.0',
  fallback: ['glib', 'libgobject_dep'])

executable('alloctest', 'alloctest.c',
  dependencies: gobject_dep)
