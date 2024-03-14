if system == 'linux'
	osi_src_dir = '../osi/os/posix'
endif

parse_base_path = join_paths(meson.current_build_dir(), 'parse')

# HACK: the EPICS flex fork exports files as "<basename>.tab.c/h" instead of
# "<basename>.c/h", so we use a custom_target that renames them afterwards.
parse_y = custom_target(
  'parse_y',
  input: ['parse.y'],
  output: ['parse.c', 'parse.h'],
  command: [
		'sh', '-c',
    yacc.full_path() + ' -b ' + parse_base_path + ' -l -d @INPUT@ && '
		+ 'mv ' + parse_base_path + '.tab.c ' + parse_base_path + '.c && '
		+ 'mv ' + parse_base_path + '.tab.h ' + parse_base_path + '.h'
  ],
	depends: yacc,
)

flex = executable(
	'flex',
	[
		'nfa.c',
		parse_y,
		'dfa.c',
		'gen.c',
		'sym.c',
		'ecs.c',
		'ccl.c',
		'misc.c',
		'tblcmp.c',
    api_header,
	],
	include_directories: ['../osi', osi_src_dir],
	native: true,
)

flex_gen = generator(
  flex,
  arguments: ['-o', '@OUTPUT@', '-8', '-I', '@INPUT@'],
  output: '@BASENAME@.c',
)
