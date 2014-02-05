# default header
import os
env = Environment(
	CXX = "clang++",
	CXXFLAGS = '-std=c++11 -g',
	CCFLAGS = ['-O3'],
	CPPPATH = [
		'LibOVR/Include',
	],
	LIBPATH = [
		'LibOVR/Lib/Linux/Release/x86_64',
	])
env['ENV']['TERM'] = os.environ['TERM']

# project specific code
env.Program(
	'construct',
	source = [
		'gl.cpp',
		'main.cpp',
		],
	LIBS = [
		'libboost_system-mt',
		'libboost_thread-mt',
		'libcairo',
		'libdl',
		'libGL',
		'libGLEW',
		'libglfw',
		'libovr',
		'libpthread',
		'libudev',
		'libv8',
		'libX11',
		'libXinerama',
		])
