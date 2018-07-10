Import("projenv")
# set CPP compiler options only
projenv.Append(CXXFLAGS=["-Wno-reorder"])
