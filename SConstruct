# default header
import os
env = Environment(
	CXX = "clang++",
	CXXFLAGS = '-std=c++11 -g -pthread',
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
		'dasher.cpp',
		'gl.cpp',
		'main.cpp',
		'scene.cpp',
		'ui.cpp',
		],
	LIBS = [
		'libboost_system-mt',
		'libboost_thread-mt',
		'libcairo',
		'libdl',
		'libGL',
		'libGLEW',
		'libglfw',
		'libjsoncpp',
		'libovr',
		'libudev',
		'libv8',
		'libX11',
		'libXinerama',
		])
