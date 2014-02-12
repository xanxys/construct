# default header
import os
env = Environment(
	CXX = "clang++",
	CXXFLAGS = '-std=c++11 -g -pthread',
	CCFLAGS = ['-O3'],
	CPPPATH = [
		'LibOVR/Include',
		'/usr/include/GL',  # GLFW in fedora
		'/usr/include/GLFW',  # GLFW in ubuntu
	],
	LIBPATH = [
		'LibOVR/Lib/Linux/Release/x86_64',
	])
env['ENV']['TERM'] = os.environ['TERM']

# project specific code
LIBS = [
	'libboost_system-mt',
	'libboost_thread-mt',
	'libcairo',
	'libdl',
	'libGL',
	'libGLEW',
	'libovr',
	'libudev',
	'libv8',
	'libX11',
	'libXinerama',
	]

env.ParseConfig('pkg-config --cflags --libs glfw3')
env.ParseConfig('pkg-config --cflags --libs jsoncpp')

# main code
env.Program(
	'construct',
	source = [env.Object(f) for f in env.Glob('*.cpp') if not f.name.endswith('_test.cpp')],
	LIBS = LIBS + env['LIBS'])

# test code
program_test = env.Program(
	'construct_test',
	source = [env.Object(f) for f in env.Glob('*.cpp') if f.name != 'main.cpp'],
	LIBS = LIBS + ['libgtest', 'libgtest_main'] + env['LIBS'])



env.Command('test', None, './' + program_test[0].path)
env.Depends('test', 'construct_test')
