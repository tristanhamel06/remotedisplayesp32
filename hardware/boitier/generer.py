# Genere le boitier pour l'ecran 2.8" (MSP2807) a partir du boitier de @NachiS
# (fait pour un ecran 2.4"), avec un bouton TL1105 sur le dessus.
#
#  arriere : display-back.stl etire de 16 mm en largeur + bouton
#  devant  : contour et trous de vis de display-front.stl, etire de 16 mm,
#            interieur refait pour le MSP2807 (fenetre, poche, 4 tiges)
#  bouton  : piston + porte-switch (le switch s'appuie sur une barre au fond)
#
# Necessite : pip install manifold3d numpy
# Lancer : python3 generer.py   (ecrit les STL dans a-imprimer/)
import numpy as np, struct, os
from manifold3d import Manifold, Mesh, set_circular_segments

set_circular_segments(96)
HERE = os.path.dirname(os.path.abspath(__file__))
ORIG = os.path.join(HERE, 'original-nachis')
OUT = os.path.join(HERE, 'a-imprimer')

def load(p):
    d = open(p, 'rb').read(); n = struct.unpack('<I', d[80:84])[0]
    a = np.frombuffer(d[84:84+n*50], dtype=np.dtype([('n', '<3f4'), ('v', '<9f4'), ('a', '<u2')]))
    v = a['v'].reshape(-1, 3).astype(np.float64)
    uniq, inv = np.unique(np.round(v, 4), axis=0, return_inverse=True)
    return Manifold(Mesh(vert_properties=uniq.astype(np.float32), tri_verts=inv.reshape(-1, 3).astype(np.uint32)))

def save(m, p):
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]; t = np.asarray(mesh.tri_verts)
    tri = v[t]
    nrm = np.cross(tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0])
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
    rec = np.zeros(len(t), dtype=np.dtype([('n', '<3f4'), ('v', '<9f4'), ('a', '<u2')]))
    rec['n'] = nrm; rec['v'] = tri.reshape(-1, 9)
    with open(p, 'wb') as f:
        f.write(b'\0'*80); f.write(struct.pack('<I', len(t))); f.write(rec.tobytes())

def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1-x0, y1-y0, z1-z0]).translate([x0, y0, z0])

def cyl_y(d, y0, y1, cx, cz):
    return Manifold.cylinder(y1-y0, d/2, d/2).rotate([-90, 0, 0]).translate([cx, y0, cz])

def cyl_z(d, z0, z1, cx, cy):
    return Manifold.cylinder(z1-z0, d/2, d/2).translate([cx, cy, z0])

def on_bed(m):
    b = m.bounding_box()
    return m.translate([-(b[0]+b[3])/2, -(b[1]+b[4])/2, -b[2]])

def stretch_x(m, x_cut, d):
    # coupe la piece au plan x = x_cut, pousse la partie de droite de d,
    # et remplit le trou avec la coupe extrudee (murs et plancher prolonges)
    left = m.trim_by_plane([-1, 0, 0], -x_cut)
    right = m.trim_by_plane([1, 0, 0], x_cut).translate([d, 0, 0])
    section = m.rotate([0, -90, 0]).slice(x_cut)          # plan x = x_cut
    fill = Manifold.extrude(section, d + 0.002).translate([0, 0, x_cut - 0.001]).rotate([0, 90, 0])
    return left + fill + right

# ---------------------------------------------------------------------------
# Repere : celui de display-back.stl. Fond a z = 0, bord a z = 30, le devant
# se pose dessus (face de l'ecran a z = 40.1). y bas = pied, y haut = dessus.
# ---------------------------------------------------------------------------
X_CUT = -31.003          # entre le bouton et la poche de l'ESP32 : juste des murs et du plancher
STRETCH = 16.0           # 94.2 mm -> 110.2 mm de large
Y_MID = (76.55 + 143.61) / 2
X_LEFT = -61.0
X_RIGHT = 33.228 + STRETCH
X_MID = (X_LEFT + X_RIGHT) / 2
RIM = 30.0               # haut de l'arriere
FRONT_T = 10.1           # epaisseur du devant

# --- Ecran MSP2807 (plan lcdwiki MSP2807_Size.pdf), a plat, pins a droite ---
PCB_L, PCB_W, PCB_T = 86.0, 50.0, 1.6
GLASS_L, GLASS_W, GLASS_T = 69.2, 50.0, 3.5      # LCD 2.3 + tactile 1.2
TAPE = 0.5                                       # ruban entre la vitre et le circuit
GLASS_FROM_PINS = 10.4                           # bord de la vitre <- bout cote pins
AA_L, AA_W = 57.6, 43.2                          # zone d'affichage
AA_FROM_PINS = 19.1
HOLES_FROM_PINS = (4.92, 83.0)                   # trous du circuit (diametre 3.2)
HOLE_Y = 22.0                                    # +/- par rapport au centre
HEADER_FROM_PINS = 2.0                           # rangee de pins
HEADER_BACK = 12.78                              # avant de la vitre -> bout des pins

# --- Devant ---
FACE = 1.2               # epaisseur devant la vitre
CLEAR = 0.3              # jeu autour du circuit
WINDOW_MARGIN = 0.6      # la fenetre depasse l'image de 0.6 mm de chaque cote
CHAMFER = 0.8            # biseau de la fenetre, cote exterieur
BOSS_D = 4.4             # appuis sous les trous du circuit
PEG_D = 2.6              # tiges dans les trous de 3.2 mm
PEG_EXTRA = 1.5          # depassement des tiges derriere le circuit

FACE_OUT = RIM + FRONT_T
FACE_IN = FACE_OUT - FACE
PCB_FRONT = FACE_IN - GLASS_T - TAPE
PCB_BACK = PCB_FRONT - PCB_T
PCB_X0 = X_MID - PCB_L/2
PCB_X1 = X_MID + PCB_L/2                         # cote pins
PINS_END = FACE_IN - HEADER_BACK

def from_pins(d):
    return PCB_X1 - d

# ---------------------------------------------------------------------------
# Arriere : etirement + bouton
# ---------------------------------------------------------------------------
back = stretch_x(load(os.path.join(ORIG, 'display-back.stl')), X_CUT, STRETCH)

INNER, OUTER = 136.78, 143.61     # mur du haut
WALL = OUTER - INNER
CX, CZ = -40.0, 17.0              # bouton : a gauche, loin des fils de l'ESP32

SW_H = 5.0                        # TL1105 : base -> bout de la tige
HOLE_D, SHAFT_D, FLANGE_D, FLANGE_T = 6.4, 6.0, 10.0, 1.2
PROTRUDE, TRAVEL, GAP = 1.5, 1.5, 0.3
POCKET, LEAD_RELIEF, BAR_W, BAR_T = 6.3, 8.4, 3.0, 1.6
CH_W, HALF, GROOVE = 10.8, 7.0, 0.8

CARRIER_FRONT = INNER - FLANGE_T - TRAVEL
SEAT = INNER - FLANGE_T - GAP - SW_H
CARRIER_BACK = SEAT - BAR_T
CH_DEPTH = INNER - CARRIER_BACK + 0.4
FLOOR_Z = CZ - CH_W/2

holder = box(CX-HALF, CX+HALF, INNER-CH_DEPTH, INNER+0.3, FLOOR_Z-1.6, CZ+HALF)
zb = FLOOR_Z - 1.6
pts = []
for x in (CX - HALF, CX + HALF):
    pts += [[x, INNER + 0.3, zb], [x, INNER - CH_DEPTH, zb], [x, INNER + 0.3, zb - CH_DEPTH - 0.3]]
holder = holder + Manifold.hull_points(pts)
channel = box(CX-CH_W/2, CX+CH_W/2, INNER-CH_DEPTH-1, INNER+0.01, FLOOR_Z, CZ+HALF+1)
grooves = box(CX-CH_W/2-GROOVE, CX+CH_W/2+GROOVE, CARRIER_BACK+0.35, CARRIER_FRONT-0.35, FLOOR_Z, CZ+HALF+1)
hole = cyl_y(HOLE_D, INNER-0.5, OUTER+1, CX, CZ)

path = box(CX-5, CX+5, INNER-FLANGE_T-WALL-PROTRUDE-0.5, INNER-CH_DEPTH, CZ-5, CZ+5)
assert (back ^ path).volume() < 1e-6, "quelque chose bloque le chemin du piston"
assert (back ^ box(CX-HALF, CX+HALF, INNER-CH_DEPTH, INNER-0.01, zb, CZ+HALF)).volume() < 1e-6, \
    "le support du bouton touche une partie existante du boitier"
back_case = (back + holder) - channel - grooves - hole

shaft_len = WALL + PROTRUDE
piston = (Manifold.cylinder(FLANGE_T, FLANGE_D/2, FLANGE_D/2)
          + Manifold.cylinder(shaft_len - 0.6, SHAFT_D/2, SHAFT_D/2).translate([0, 0, FLANGE_T])
          + Manifold.cylinder(0.6, SHAFT_D/2, SHAFT_D/2 - 0.6).translate([0, 0, FLANGE_T + shaft_len - 0.6]))

car_w, car_top, tab = CH_W - 0.2, CZ + HALF - 0.2, GROOVE - 0.15
carrier = (box(CX-car_w/2, CX+car_w/2, CARRIER_BACK, CARRIER_FRONT, FLOOR_Z, car_top)
           + box(CX-car_w/2-tab, CX+car_w/2+tab, CARRIER_BACK+0.5, CARRIER_FRONT-0.5, FLOOR_Z, car_top))
carrier -= box(CX-POCKET/2, CX+POCKET/2, SEAT, CARRIER_FRONT+1, CZ-POCKET/2, CZ+POCKET/2)
carrier -= box(CX-LEAD_RELIEF/2, CX+LEAD_RELIEF/2, SEAT, SEAT+1.0, CZ-LEAD_RELIEF/2, CZ+LEAD_RELIEF/2)
carrier -= box(CX-LEAD_RELIEF/2, CX+LEAD_RELIEF/2, CARRIER_BACK-1, SEAT+0.01, CZ+BAR_W/2, CZ+LEAD_RELIEF/2)
carrier -= box(CX-LEAD_RELIEF/2, CX+LEAD_RELIEF/2, CARRIER_BACK-1, SEAT+0.01, CZ-LEAD_RELIEF/2, CZ-BAR_W/2)

# ---------------------------------------------------------------------------
# Devant : contour de @NachiS (retourne et pose sur l'arriere), etire,
# interieur rempli puis refait pour le MSP2807
# ---------------------------------------------------------------------------
front = load(os.path.join(ORIG, 'display-front.stl')).rotate([180, 0, 0]).translate([-91.0, 147.6, RIM + FRONT_T])
front = stretch_x(front, X_CUT, STRETCH)

# on bouche l'ancienne fenetre et les anciennes poches (faites pour le 2.4")
front += box(-57.4, 20.7 + STRETCH, 87.8, 132.4, RIM, FACE_OUT)

# poche du circuit et de la vitre (ouverte vers l'arriere)
front -= box(PCB_X0-CLEAR, PCB_X1+CLEAR, Y_MID-PCB_W/2-CLEAR, Y_MID+PCB_W/2+CLEAR, RIM-1, FACE_IN)

# fenetre, avec un biseau vers l'exterieur
wx0 = from_pins(AA_FROM_PINS + AA_L) - WINDOW_MARGIN
wx1 = from_pins(AA_FROM_PINS) + WINDOW_MARGIN
wy0, wy1 = Y_MID - AA_W/2 - WINDOW_MARGIN, Y_MID + AA_W/2 + WINDOW_MARGIN
c = CHAMFER
win = Manifold.hull_points([[x, y, z] for x, y, z in
    [(wx0, wy0, FACE_IN-0.1), (wx1, wy0, FACE_IN-0.1), (wx0, wy1, FACE_IN-0.1), (wx1, wy1, FACE_IN-0.1),
     (wx0, wy0, FACE_OUT-c), (wx1, wy0, FACE_OUT-c), (wx0, wy1, FACE_OUT-c), (wx1, wy1, FACE_OUT-c),
     (wx0-c-0.1, wy0-c-0.1, FACE_OUT+0.1), (wx1+c+0.1, wy0-c-0.1, FACE_OUT+0.1),
     (wx0-c-0.1, wy1+c+0.1, FACE_OUT+0.1), (wx1+c+0.1, wy1+c+0.1, FACE_OUT+0.1)]])
front -= win

# appuis + tiges dans les 4 trous du circuit
for d in HOLES_FROM_PINS:
    for sy in (-1, 1):
        hx, hy = from_pins(d), Y_MID + sy*HOLE_Y
        front += cyl_z(BOSS_D, PCB_FRONT, FACE_IN + 0.01, hx, hy)
        front += cyl_z(PEG_D, PCB_BACK - PEG_EXTRA, PCB_FRONT + 0.01, hx, hy)

assert PCB_BACK - PEG_EXTRA > RIM, "les tiges depassent dans l'arriere"

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for name, m in [('arriere.stl', back_case), ('devant.stl', front), ('bouton-piston.stl', piston),
                    ('bouton-porte-switch.stl', carrier)]:
        assert str(m.status()).endswith('NoError'), name
    save(back_case, os.path.join(OUT, 'arriere.stl'))
    save(on_bed(front.rotate([180, 0, 0])), os.path.join(OUT, 'devant.stl'))  # face de l'ecran sur le plateau
    save(piston, os.path.join(OUT, 'bouton-piston.stl'))          # collerette sur le plateau
    save(on_bed(carrier.rotate([90, 0, 0])), os.path.join(OUT, 'bouton-porte-switch.stl'))  # barre sur le plateau
    bb = back_case.bounding_box()
    print(f"boitier : {bb[3]-bb[0]:.1f} x {bb[4]-bb[1]:.1f} x {bb[5]-bb[2] + FRONT_T:.1f} mm (avec le devant)")
    print(f"fenetre : {wx1-wx0:.1f} x {wy1-wy0:.1f} mm, decalee de {(wx0+wx1)/2 - X_MID:+.1f} mm du centre")
