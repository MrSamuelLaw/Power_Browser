get_ipython().run_line_magic("reset", " -f")


import sympy as sm
import numpy as np
import matplotlib.pyplot as plt


# create expression for velocity
d, t, s = sm.symbols("d, t, s")
r = d/2        # cylinder radius in km
w = sm.pi*2/t  # cylinder rad/micros
v = w*r        # cylinder km/micros
v


# create an expression for mph
vals = {d: 054.2e-6  } # km
kmm = v.subs(vals)     # km/micros
kms = kmm*1e6          # km/s
kmh = kms*3600         # km/h
mph = kmh*0.621371     # mi/h
mph


# create a function to calculate mph from micros
mph_func = sm.lambdify([t], mph, modules="numpy")
mph_func(1_000_000)


# create a function to calcualte power from mph
power = (5.244820*mph) + (0.019168*mph*mph*mph)
power_func = sm.lambdify([t], power, modules="numpy")
power_func(1_000_000)


# plot the results using a test dataset
micros = np.linspace(2.5e4, 1e6, 1000)
speeds = [mph_func(m) for m in micros]
powers = [power_func(m) for m in micros]
plt.plot(speeds, powers)
plt.grid();


# create the javascript code
from sympy.printing.jscode import jscode
print(f'mph = {jscode(mph)}')
print(f'watts = {jscode(power)}')
