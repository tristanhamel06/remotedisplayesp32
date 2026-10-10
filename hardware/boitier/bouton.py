# Ajoute un bouton TL1105 (tige standard) sur le dessus du boitier de @NachiS.
#  - trou de 6.4 mm dans le mur du haut, a gauche (loin des fils de l'ESP32)
#  - derriere le mur : un canal en U ouvert par le haut et du cote interieur
#  - piston imprime : traverse le mur, depasse de 1.5 mm, collerette en dedans
#  - porte-switch (v2) : poche carree pour le switch, avec une barre au fond
#    sur laquelle le switch s'appuie. La force du bouton va dans le plastique,
#    pas dans la colle, et la position du switch est fixe.
#
# Necessite : pip install manifold3d numpy
# Lancer : python3 bouton.py (dans ce dossier). Regenere les 3 STL.
import numpy as np, struct, os
from manifold3d import Manifold, Mesh, set_circular_segments

set_circular_segments(96)
B = os.path.dirname(os.path.abspath(__file__)) + '/'

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
    # cylindre d'axe Y, de y0 a y1
    return Manifold.cylinder(y1-y0, d/2, d/2).rotate([-90, 0, 0]).translate([cx, y0, cz])

def on_bed(m):
    # pose une piece sur le plateau (z = 0), centree en x/y
    b = m.bounding_box()
    return m.translate([-(b[0]+b[3])/2, -(b[1]+b[4])/2, -b[2]])

# --- Mesures du boitier (mesurees dans display-back.stl) ---
INNER = 136.78          # face interieure du mur du haut
OUTER = 143.61          # face exterieure
WALL = OUTER - INNER    # 6.83 mm
CX = -40.0              # position en largeur : a gauche, entre le bossage du coin
                        # (jusqu'a x = -48) et l'ESP32 (a partir de x = -28)
CZ = 17.0               # hauteur du bouton (fond a z = 2, bord a z = 30)

# --- TL1105 (fiche technique E-Switch) ---
SW_H = 5.0              # de la base jusqu'au bout de la tige
SW_BODY_H = 3.6         # hauteur du corps
SW_LEADS = 7.9          # largeur des pattes pliees, a la base

# --- Parametres ---
HOLE_D = 6.4            # trou dans le mur
SHAFT_D = 6.0           # tige du piston
FLANGE_D = 10.0         # collerette du piston
FLANGE_T = 1.2
PROTRUDE = 1.5          # le piston depasse de 1.5 mm du boitier
TRAVEL = 1.5            # course max du piston (le switch clique apres 0.25 mm)
GAP = 0.3               # jeu entre la tige du switch et la collerette, au repos
POCKET = 6.3            # poche carree pour le corps 6 x 6 mm
LEAD_RELIEF = 8.4       # degagement pour les pattes pliees, a la base
BAR_W = 3.0             # barre du fond (le switch s'appuie dessus)
BAR_T = 1.6
CH_W = 10.8             # largeur interieure du canal
HALF = 7.0              # le support fait 14 mm de large
GROOVE = 0.8            # profondeur des rainures dans les murs du canal

# Positions le long de l'axe du bouton (y)
CARRIER_FRONT = INNER - FLANGE_T - TRAVEL       # face avant du porte-switch
SEAT = INNER - FLANGE_T - GAP - SW_H            # base du switch (sur la barre)
CARRIER_BACK = SEAT - BAR_T
CH_DEPTH = INNER - CARRIER_BACK + 0.4           # profondeur du canal
FLOOR_Z = CZ - CH_W/2                           # fond du canal

back = load(B + 'display-back.stl')

# --- Boitier : canal en U + renfort a 45 degres + trou ---
holder = box(CX-HALF, CX+HALF, INNER-CH_DEPTH, INNER+0.3, FLOOR_Z-1.6, CZ+HALF)
zb = FLOOR_Z - 1.6
pts = []
for x in (CX - HALF, CX + HALF):
    pts += [[x, INNER + 0.3, zb], [x, INNER - CH_DEPTH, zb], [x, INNER + 0.3, zb - CH_DEPTH - 0.3]]
holder = holder + Manifold.hull_points(pts)

channel = box(CX-CH_W/2, CX+CH_W/2, INNER-CH_DEPTH-1, INNER+0.01, FLOOR_Z, CZ+HALF+1)
grooves = box(CX-CH_W/2-GROOVE, CX+CH_W/2+GROOVE,
              CARRIER_BACK+0.35, CARRIER_FRONT-0.35, FLOOR_Z, CZ+HALF+1)
hole = cyl_y(HOLE_D, INNER-0.5, OUTER+1, CX, CZ)

# Le piston doit pouvoir glisser de l'interieur jusque dans le trou
path = box(CX-5, CX+5, INNER-FLANGE_T-WALL-PROTRUDE-0.5, INNER-CH_DEPTH, CZ-5, CZ+5)
assert (back ^ path).volume() < 1e-6, "quelque chose bloque le chemin du piston"
assert (back ^ box(CX-HALF, CX+HALF, INNER-CH_DEPTH, INNER-0.01, zb, CZ+HALF)).volume() < 1e-6, \
    "le support touche une partie existante du boitier"

case = (back + holder) - channel - grooves - hole
print('boitier      :', case.status(), 'genus', case.genus())
save(case, B + 'display-back-bouton.stl')

# --- Piston (imprime collerette a plat sur le plateau) ---
shaft_len = WALL + PROTRUDE
piston = (Manifold.cylinder(FLANGE_T, FLANGE_D/2, FLANGE_D/2)
          + Manifold.cylinder(shaft_len - 0.6, SHAFT_D/2, SHAFT_D/2).translate([0, 0, FLANGE_T])
          + Manifold.cylinder(0.6, SHAFT_D/2, SHAFT_D/2 - 0.6).translate([0, 0, FLANGE_T + shaft_len - 0.6]))
print('piston       :', piston.status(), 'hauteur', round(FLANGE_T + shaft_len, 2), 'mm')
save(piston, B + 'bouton-piston.stl')

# --- Porte-switch v2 (construit en place dans le boitier) ---
car_w = CH_W - 0.2
car_top = CZ + HALF - 0.2
tab = GROOVE - 0.15
carrier = (box(CX-car_w/2, CX+car_w/2, CARRIER_BACK, CARRIER_FRONT, FLOOR_Z, car_top)
           + box(CX-car_w/2-tab, CX+car_w/2+tab, CARRIER_BACK+0.5, CARRIER_FRONT-0.5, FLOOR_Z, car_top))
# poche du corps
carrier -= box(CX-POCKET/2, CX+POCKET/2, SEAT, CARRIER_FRONT+1, CZ-POCKET/2, CZ+POCKET/2)
# degagement pour les pattes pliees (1 mm au-dessus de la base)
carrier -= box(CX-LEAD_RELIEF/2, CX+LEAD_RELIEF/2, SEAT, SEAT+1.0, CZ-LEAD_RELIEF/2, CZ+LEAD_RELIEF/2)
# fond ouvert au-dessus et au-dessous de la barre : les pattes passent par la
carrier -= box(CX-LEAD_RELIEF/2, CX+LEAD_RELIEF/2, CARRIER_BACK-1, SEAT+0.01, CZ+BAR_W/2, CZ+LEAD_RELIEF/2)
carrier -= box(CX-LEAD_RELIEF/2, CX+LEAD_RELIEF/2, CARRIER_BACK-1, SEAT+0.01, CZ-LEAD_RELIEF/2, CZ-BAR_W/2)
print('porte-switch :', carrier.status(), 'epaisseur', round(CARRIER_FRONT-CARRIER_BACK, 2), 'mm')
# imprime a plat, la barre sur le plateau
save(on_bed(carrier.rotate([90, 0, 0])), B + 'bouton-porte-switch.stl')
