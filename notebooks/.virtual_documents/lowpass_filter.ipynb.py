get_ipython().run_line_magic("reset", " -f")


import math as m


# input known values
wheel_diameter = 2100e-3     # m
cylinder_diameter = 76.2e-3  # m
mps_max = 15.65              # m/s

# calculate omega of wheel using v = wr 
wheel_rps = mps_max/(0.5*wheel_diameter)

# calculate omega of cylinder using |w_cylinder| = gr*|w_wheel|
cylinder_rps = wheel_rps * (wheel_diameter/cylinder_diameter)

# print out the results
f"{cylinder_rps = :0.2f} hz"


R = 50e3  # ohms
C = 5e-9 # farads
f_cutoff = 1/(2*m.pi*R*C)
f"{f_cutoff = :0.2f} hz"
