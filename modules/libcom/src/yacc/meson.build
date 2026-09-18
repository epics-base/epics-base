if system == 'linux'
	osi_src_dir = '../osi/os/posix'
endif

yacc = executable(
	'yacc',
	[
    'closure.c',
    'antelope.c',
    'output.c',
    'verbose.c',
    'mkpar.c',
    'error.c',
    'symtab.c',
    'lalr.c',
    'skeleton.c',
    'lr0.c',
    'warshall.c',
    'reader.c',
    api_header,
	],
	include_directories: ['../osi', osi_src_dir],
	native: true,
)

yacc_gen = generator(
  yacc,
  arguments: ['-b', '@BUILD_DIR@/@BASENAME@', '@INPUT@'],
  output: ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
)
