#
# Calculates some values used in the PDLC driver design.
#

def calc_boost_uvlo():
    Vsupply_on = 11.5
    Vsupply_off = 10.6
    Vuvlo_rising = 1.5
    Vuvlo_falling = 1.45
    Iuvlo = 0.000005
    Ruvlo_t = (Vsupply_on * Vuvlo_falling / Vuvlo_rising - Vsupply_off) / Iuvlo
    Ruvlo_b = Vuvlo_rising * Ruvlo_t / (Vsupply_on - Vuvlo_rising)

    def Vsupply_on_actual():
        return (Vuvlo_rising * Ruvlo_t / Ruvlo_b) + Vuvlo_rising

    def Vsupply_off_actual():
        return Vsupply_on_actual() * Vuvlo_falling / Vuvlo_rising - Ruvlo_t * Iuvlo 

    def print_values():
        print(f'  Ruvlo_t = {Ruvlo_t} ohm')
        print(f'  Ruvlo_b = {Ruvlo_b} ohm')
        print(f'  Vsupply_on_actual = {Vsupply_on_actual()} V')
        print(f'  Vsupply_off_actual = {Vsupply_off_actual()} V')

    print('\nBOOST CONVERTER UVLO\n\nParameters:')
    print(f'  Vsupply_on = {Vsupply_on} V')
    print(f'  Vsupply_off = {Vsupply_off} V')
    print(f'  Vuvlo_rising = {Vuvlo_rising} V')
    print(f'  Vuvlo_falling = {Vuvlo_falling} V')
    print(f'  Iuvlo = {Iuvlo} V')
    print('\nIdeal values:')
    print_values()
    print('\nSelected values:')
    Ruvlo_t = 100000
    Ruvlo_b = 15000
    print_values()


def calc_buck_feedback():
    Vout_low = 3
    Vout_high = 63
    Vdda = 3.3
    Vadj_margin = 0.3
    Vadj_min = 0.2
    Vadj_max = Vdda - 0.2
    Vadj_low = 0 + Vadj_margin
    Vadj_high = Vdda - Vadj_margin
    Vfb_ref = 1
    Rfb_t = 220 # LMR380x0 data sheet suggests 100 <= Rfb_t < 1000
    Rfb_b = -Vfb_ref * Rfb_t * (Vadj_low - Vadj_high) / ((Vout_low - Vout_high + Vadj_low - Vadj_high) * Vfb_ref - (Vadj_low * Vout_low) + (Vadj_high * Vout_high))
    Rfb_a = Rfb_b * Rfb_t * (Vadj_high - Vfb_ref) / ((Rfb_b * Vfb_ref) + (Rfb_t * Vfb_ref) - (Rfb_b * Vout_low))

    def Vout(Vadj):
        return Vfb_ref * (1 + Rfb_t / Rfb_b) - (Rfb_t / Rfb_a) * (Vadj - Vfb_ref)

    def dac_code(Vadj):
        return round(4095 * Vadj / Vdda)

    def print_values():
        print(f'  Vadj_low = {Vadj_low} V')
        print(f'  Vadj_high = {Vadj_high} V')
        print(f'  Rfb_t = {Rfb_t} kOhm')
        print(f'  Rfb_b = {Rfb_b} kOhm')
        print(f'  Rfb_a = {Rfb_a} kOhm')
        print(f'  Vout(Vadj_low) = {Vout(Vadj_low)} V')
        print(f'  Vout(Vadj_high) = {Vout(Vadj_high)} V')

    def print_point(label, Vadj):
        print(f'| {label:5} | {Vadj:4.2f} V | {Vout(Vadj): 6.2f} V | {dac_code(Vadj):8d} |')

    print('\nBUCK CONVERTER FEEDBACK NETWORK\n\nParameters:')
    print(f'  Vout_low = {Vout_low}')
    print(f'  Vout_high = {Vout_high}')
    print('\nIdeal values:')
    print_values()
    print('\nSelected values:')
    Rfb_b = 4.7
    Rfb_a = 10
    Vadj_low = 0.309
    Vadj_high = Vdda - 0.263
    Vadj_mid = (Vadj_low + Vadj_high) / 2
    Vadj_rms = Vadj_high + (Vadj_low - Vadj_high) / 2**.5
    print_values()
    print('\nOperating points:\n')
    print('| label | Vadj   | Vout     | DAC code |')
    print('| ----- | ------ | -------- | -------- |')
    print_point('0', 0)
    print_point('min', Vadj_min)
    print_point('low', Vadj_low)
    print_point('mid', Vadj_mid)
    print_point('rms', Vadj_rms)
    print_point('high', Vadj_high)
    print_point('max', Vadj_max)
    print_point('Vdda', Vdda)


calc_boost_uvlo()
calc_buck_feedback()
