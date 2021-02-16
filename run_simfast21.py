import subprocess
import numpy as np
from skopt.sampler import Lhs


def make_ini_file(ini_file,fname,omega_m,h,sigma_8,C_ion,D_ion,f_esc):
    '''

    '''
    #Hard coded to positions in original simfast21.ini file
    ini_file[30] = 'omega_matter = ' + str(omega_m) + '\n'
    ini_file[33] = 'hubble = ' + str(h) + '\n'
    ini_file[36] = 'sigma8 = ' + str(sigma_8) + '\n'

    ini_file[41] = 'Cion = ' + str(C_ion) + '\n'
    ini_file[42] = 'Dion = ' + str(D_ion) + '\n'
    ini_file[69] = 'fesc = ' + str(f_esc) + '\n'

    new_file = open(fname,'w')
    new_file.writelines(ini_file)
    new_file.close()

    print('Wrote file to ',fname)


omega_m_limits = [0.2,0.4]
h_limits = [0.6,0.8]
sigma_8_limits = [0.7,0.8]

f_esc_limits = [0.01,1.]
C_ion_limits = [0.,1.]
D_ion_limits = [0.,2.]

limits = np.array([omega_m_limits,h_limits,sigma_8_limits,f_esc_limits,C_ion_limits,D_ion_limits])

lhs = Lhs(lhs_type="classic", criterion=None)

np.random.seed(123456789)
omega_m, h, sigma_8, f_esc, C_ion, D_ion = np.array(lhs.generate(limits, n_samples= 1000)).T

file = open("simfast21.ini")
ini_file = file.readlines()
file.close()


for i in range(len(omega_m)):

    dirname = 'runs/run'+str(i)
    os.system('mkdir '+dirname)

    fname = dirname+'/simfast21.ini'

    make_ini_file(ini_file, fname, omega_m = omega_m[i], h = h[i], sigma_8=sigma_8[i],
                  C_ion=C_ion[i], D_ion=D_ion[i], f_esc=f_esc[i])


    process = subprocess.Popen(['./simfast21', dirname],
                               stdout=subprocess.PIPE,
                               universal_newlines=True)

    while True:
        output = process.stdout.readline()
        if 'Generating' in output:
            print(output.strip())

        return_code = process.poll()
        if return_code is not None:
            print('RETURN CODE', return_code)
            # Process has finished, read rest of the output
            for output in process.stdout.readlines():
                print(output.strip())
            break
