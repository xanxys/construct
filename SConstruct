# default header
import os
env = Environment(
	CXX = "clang++",
	CXXFLAGS = '-std=c++11',
	CCFLAGS = ['-O3'],
	CPPPATH = [
		'OculusSDK/LibOVR/Include',
	],
	LIBPATH = [
		'OculusSDK/LibOVR/Lib/Linux/Release/x86_64',
	])
env['ENV']['TERM'] = os.environ['TERM']

# project specific code
env.Program(
	'construct',
	source = [
		'main.cpp',
		],
	LIBS = [
		'libboost_system-mt',
		'libboost_thread-mt',
		'libdl',
		'libGL',
		'libglfw',
		'libovr',
		'libpthread',
		'libudev',
		'libX11',
		'libXinerama',
		])
