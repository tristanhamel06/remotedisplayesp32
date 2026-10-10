# Ajoute un bouton TL1105 (tige courte) sur le dessus du boitier de @NachiS.
#  - trou de 6.4 mm dans le mur du haut
#  - support derriere le mur : cavite pour la collerette du piston (ouverte
#    par le haut pour l'installer) + tube carre qui tient le switch
#  - piston imprime separement : tige 6 mm qui depasse de 1.5 mm, collerette 10 mm
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

# --- Mesures du boitier (mesurees dans display-back.stl) ---
INNER = 136.78          # face interieure du mur du haut
OUTER = 143.61          # face exterieure
WALL = OUTER - INNER    # 6.83 mm
CX = (-61.0 + 33.228) / 2   # centre en largeur
CZ = 17.0               # hauteur du bouton (fond a z=2, bord a z=30)

# --- Parametres ---
HOLE_D = 6.4            # trou dans le mur
SHAFT_D = 6.0           # tige du piston
FLANGE_D = 10.0         # collerette du piston
FLANGE_T = 1.2
CAV_D = 10.8            # cavite de la collerette
CAV_DEPTH = 2.0         # 1.2 collerette + 0.8 de course
TUBE = 6.3              # tube carre pour le switch 6x6
TUBE_LEN = 3.0
HALF = 7.0              # support : 14 x 14 mm
PROTRUDE = 1.5          # le piston depasse de 1.5 mm du boitier

back = load(B + 'display-back.stl')

# Le support est un canal en U ouvert par le haut et du cote interieur :
# le piston s'installe de l'interieur, puis le porte-switch se glisse dans
# deux rainures par le haut et retient la collerette.
CH_W = 10.8             # largeur interieure du canal
CH_DEPTH = 6.0          # profondeur du canal (a partir du mur)
FLOOR_Z = CZ - CH_W/2   # fond du canal (0.4 mm sous la collerette)
CARRIER_FRONT = INNER - CAV_DEPTH   # face avant du porte-switch
CARRIER_T = 3.0         # epaisseur du porte-switch
GROOVE = 0.8            # profondeur des rainures dans les murs du canal

holder = box(CX-HALF, CX+HALF, INNER-CH_DEPTH, INNER+0.3, FLOOR_Z-1.6, CZ+HALF)
zb = FLOOR_Z - 1.6
pts = []
for x in (CX - HALF, CX + HALF):
    pts += [[x, INNER + 0.3, zb], [x, INNER - CH_DEPTH, zb], [x, INNER + 0.3, zb - CH_DEPTH - 0.3]]
holder = holder + Manifold.hull_points(pts)   # renfort a 45 degres

channel = box(CX-CH_W/2, CX+CH_W/2, INNER-CH_DEPTH-1, INNER+0.01, FLOOR_Z, CZ+HALF+1)
grooves = box(CX-CH_W/2-GROOVE, CX+CH_W/2+GROOVE,
              CARRIER_FRONT-CARRIER_T+0.35, CARRIER_FRONT-0.35, FLOOR_Z, CZ+HALF+1)
hole = cyl_y(HOLE_D, INNER-0.5, OUTER+1, CX, CZ)

# Espace libre pour glisser le piston de l'interieur ?
path = box(CX-5, CX+5, INNER-FLANGE_T-WALL-PROTRUDE-0.5, INNER-CH_DEPTH, CZ-5, CZ+5)
print('matiere sur le chemin du piston :', round((back ^ path).volume(), 3))

result = (back + holder) - channel - grooves - hole
print('boitier :', result.status(), 'genus', result.genus(), 'ajout', round(result.volume()-back.volume(), 1))
save(result, B + 'display-back-bouton.stl')

# --- Porte-switch (imprime a plat) ---
# Bloc qui tombe dans le canal ; trou carre de 6.3 mm pour le TL1105.
CAR_W = CH_W - 0.2
CAR_H = (CZ + HALF) - FLOOR_Z - 0.2
TAB = GROOVE - 0.15
carrier = (box(-CAR_W/2, CAR_W/2, 0, CAR_H, 0, CARRIER_T)
           + box(-CAR_W/2-TAB, CAR_W/2+TAB, 0, CAR_H, 0.5, CARRIER_T-0.5)
           - box(-TUBE/2, TUBE/2, CZ-FLOOR_Z-TUBE/2, CZ-FLOOR_Z+TUBE/2, -1, CARRIER_T+1))
print('porte-switch :', carrier.status(), round(CAR_W+2*TAB, 2), 'x', round(CAR_H, 2), 'x', CARRIER_T, 'mm')
save(carrier, B + 'bouton-porte-switch.stl')

# --- Piston (imprime collerette a plat sur le plateau) ---
shaft_len = WALL + PROTRUDE
piston = (Manifold.cylinder(FLANGE_T, FLANGE_D/2, FLANGE_D/2)
          + Manifold.cylinder(shaft_len - 0.6, SHAFT_D/2, SHAFT_D/2).translate([0, 0, FLANGE_T])
          + Manifold.cylinder(0.6, SHAFT_D/2, SHAFT_D/2 - 0.6).translate([0, 0, FLANGE_T + shaft_len - 0.6]))
print('piston :', piston.status(), 'hauteur', round(FLANGE_T + shaft_len, 2), 'mm')
save(piston, B + 'bouton-piston.stl')
