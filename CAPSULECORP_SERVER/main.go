package main

import (
	"fmt"
	"log"
	"net/http"
	"path"
	"strings"
	"time"
)

// Minimal, secure in-memory web server serving a hardcoded index and two virtual
// directories: /~bulma/ and /~trunks/. No filesystem access, no dynamic loading.

var indexHTML = `
<!DOCTYPE html>
<html lang="it">

<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CapsCorp Labs - West City</title>
  <style>
    /* Reset base */
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
      font-family: sans-serif;
    }

    body {
      background: linear-gradient(to bottom, #ffffff, #f1f5f9);
      color: #1e293b;
    }

    a {
      text-decoration: none;
      color: inherit;
    }

    /* Navbar */
    header {
      position: sticky;
      top: 0;
      background: rgba(255, 255, 255, 0.9);
      border-bottom: 1px solid #e2e8f0;
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 1rem 2rem;
      backdrop-filter: blur(8px);
    }

    nav a {
      margin-left: 1rem;
      font-weight: 500;
      transition: 0.2s;
    }

    nav a:hover {
      color: #06b6d4;
    }

    /* Hero */
    .hero {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      padding: 4rem 2rem;
      position: relative;
      overflow: hidden;
    }

    .hero h1 {
      font-size: 3rem;
      font-weight: 900;
      margin-bottom: 1rem;
    }

    .hero h1 span {
      color: #06b6d4;
    }

    .hero p {
      margin-bottom: 2rem;
      color: #475569;
    }

    .hero button {
      padding: 0.75rem 1.5rem;
      border-radius: 9999px;
      border: none;
      background: #06b6d4;
      color: #fff;
      font-weight: 600;
      cursor: pointer;
      transition: 0.2s;
    }

    .hero button:hover {
      filter: brightness(1.1);
    }

    /* Features */
    .features {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 1.5rem;
      padding: 4rem 2rem;
    }

    .feature {
      background: #fff;
      border: 1px solid #e2e8f0;
      border-radius: 2rem;
      padding: 2rem;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.05);
      transition: 0.3s;
    }

    .feature:hover {
      box-shadow: 0 6px 12px rgba(0, 0, 0, 0.1);
    }

    .feature h3 {
      font-size: 1.25rem;
      font-weight: 700;
      margin-bottom: 0.5rem;
    }

    .feature p {
      color: #475569;
    }

    /* Products */
    .products {
      background: #f1f5f9;
      border-top: 1px solid #e2e8f0;
      border-bottom: 1px solid #e2e8f0;
      padding: 4rem 2rem;
    }

    .products h2 {
      font-size: 2.25rem;
      font-weight: 900;
      margin-bottom: 2rem;
    }

    .product-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 1.5rem;
    }

    .product {
      background: #fff;
      border: 1px solid #e2e8f0;
      border-radius: 2rem;
      padding: 1.5rem;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.05);
      transition: 0.3s;
    }

    .product:hover {
      box-shadow: 0 6px 12px rgba(0, 0, 0, 0.1);
    }

    .product h3 {
      font-size: 1.25rem;
      font-weight: 700;
      margin-bottom: 0.5rem;
    }

    .product p {
      color: #475569;
      margin-bottom: 0.5rem;
    }

    /* Footer */
    footer {
      border-top: 1px solid #e2e8f0;
      background: #fff;
      padding: 2rem;
      text-align: center;
      font-size: 0.875rem;
      color: #64748b;
    }

    /* Circles decorativi */
    .circle {
      position: absolute;
      border-radius: 50%;
      filter: blur(100px);
      opacity: 0.3;
    }

    .circle.cyan {
      background: #06b6d4;
    }

    .circle.black {
      background: #000;
    }
  </style>
</head>

<body>

  <header>
    <div style="display:flex; align-items:center; gap:0.5rem; font-weight:bold;">
      <div
        style="width:32px; height:32px; border-radius:50%; background:#000; display:flex; align-items:center; justify-content:center; color:#06b6d4; font-size:14px; font-weight:bold;">
        C</div>
      CapsCorp Labs - West city
    </div>
    <nav>
      <a href="#products">Our products</a>
      <a href="#contact">Contact us</a>
    </nav>
  </header>

  <section class="hero">
    <div>
      <h1>Capsules <span>for every purpose</span></h1>
      <p>We produce capsules and items for every aspect of your daily life</p>
      <button>Explore products</button>
    </div>
    <div
      style="width:300px; height:300px; border-radius:50%; background:#06b6d4; opacity:0.2; position:absolute; top:-50px; right:-50px;">
    </div>
  </section>

  <section id="features" class="features">
    <div class="feature">
      <h3>Capsule technology</h3>
      <p>Modular design, immediate portability, no clutter</p>
    </div>
    <div class="feature">
      <h3>Engineering from the future</h3>
      <p>Advanced fabrics and great design for a modern look</p>
    </div>
    <div class="feature">
      <h3>Minimal interface</h3>
      <p>Focus on usability and usage, designed for humans and Saiyans</p>
    </div>
  </section>

  <section id="products" class="products">
    <h2>Our products</h2>
    <div class="product-grid">
      <div class="product">
        <h3>House capsule</h3>
        <p>Series H • 20 m² deployable</p>
      </div>
      <div class="product">
        <h3>Car capsule</h3>
        <p>Series W • with advanced auotmotive</p>
      </div>
      <div class="product">
        <h3>Time machine</h3>
        <p>Series M • Aero‑Drive asset</p>
      </div>
    </div>
  </section>

  <section id="contact" style="padding:4rem 2rem; text-align:center;">
    <h2>Contact us</h2>
    <!-- <a href="/~bulma" title="Contact Bulma">Contact Bulma</a> -->
    <!-- <a href="/~trunks" title="Contact Trunks">Contact Trunks</a> -->
    <p>This feature is currently not available as we are running from Red Ribbon</p>
  </section>

  <footer>
    Beware of the cyborgs!
  </footer>

</body>

</html>
`

// Hardcoded file contents. Keys are filenames served under each virtual dir.
var sites = map[string]map[string]struct {
	content string
	modtime time.Time
}{
	"bulma": {
		"notes1.txt": {content: `

Project: Temporal Capsule – Time Travel Unit
Energy system recalibration log.

The time machine’s core must be stabilized at 1.21 Zetta-Joules equivalent. Impossible to supply with conventional cells. I’ve repurposed the last working reactor cells from Capsule Corp’s vault.
Issue: Charge loss is exponential when the machine is idle. Need to design an auto-hibernate mode—only core vitals active, rest offline.

Trunks must not be stranded. If he can’t return, he’s alone.

Optimization calculations

Reactor cycles = 40 minutes for full charge.

Output conversion efficiency: 67.4% (still too low).

Drafting parallel capacitor arrays using salvaged Namekian alloy (found in storage). Alloy tolerates stress > 8000 Kelvin.

If charge efficiency doesn’t improve, Trunks will not have enough to make the return jump.



		Project: Temporal Capsule - Time Travel Unit
Energy system recalibration log.

Rule: Time traveler MUST know absolute time of departure.
If he forgets the date, re-entry alignment may fail. Misalignment = catastrophic. If he returns to wrong moment, paradox potential → collapse of local timeline.

Warning to Trunks:

When you arrive in the past, always record the date. Keep it safe, tattoo it in your mind if you must.

Your re-entry coordinates depend on that number. Forget it, and you’ll drift into the wrong year, maybe the wrong reality.

Do NOT assume “close enough” works. It doesn’t.

Time doesn’t forgive.
This isn’t just physics. It’s survival.

Machine notes:

Main drive: Gravitational folding engine (stabilized with Katchin casing).

Control interface: manual only. No A.I.—too risky. Androids could exploit.

Pilot protocol: engage anchor beacon before jump.

Energy audit:

Departure cost: 100% charge.

Return cost: 92% (margin for emergencies only).

Meaning: One trip forward, one trip back. No retries.
`, modtime: time.Date(
			2045, 9, 5, 20, 34, 58, 351267237, time.UTC)},
		"schematic.svg": {content: `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="600" height="300" viewBox="0 0 600 300">
  <rect x="10" y="10" width="580" height="280" fill="none" stroke="#000"/>
  <circle cx="150" cy="150" r="60" fill="none" stroke="#000"/>
  <rect x="300" y="100" width="180" height="100" fill="none" stroke="#000"/>
  <text x="140" y="150" font-size="12">Energy Core</text>
  <text x="320" y="150" font-size="12">Capacitor Array</text>
</svg>`, modtime: time.Date(2045, 9, 1, 8, 0, 0, 0, time.UTC)}},
	"trunks": {
		"august.txt": {content: `
07.08.2034
The sky has turned a rusted gray. Smoke never leaves the horizon.
I trained all night again. Even with Mother’s new modifications to the gravity chamber, I still can’t close the gap. The Androids… they don’t tire. They don’t doubt.
Today I watched them burn down the eastern block. There were still families there, hiding. I tried to reach them—

10.08.2034
The resistance is nearly gone. I buried three fighters today. I said their names aloud, though no one else remained to hear me. Someone should remember.
Mother tells me not to lose hope. She works tirelessly, scavenging, building, planning. But I see the exhaustion in her eyes. She’s aged so much.
Sometimes… I wonder if I’ve already failed.

14.08.2034
They came for West City again. I intercepted them near the old Capsule Corp satellite facility.
I held them back… barely. My arm still shakes as I write this. The pain is sharp.
But I saw them smile. They enjoyed it.
If Father were here… if Goku were alive… would this nightmare even exist? I won’t give up.

18.08.2034
I dreamed of Father last night. His voice was distant, but I felt his pride. Maybe it was just my imagination.
Every day feels like a borrowed hour. Still, when I lift my sword, I swear I feel all of them with me—Father, Gohan, even Goku.
I can’t shake this thought: there must be another way. A path I haven’t seen yet. Mother speaks of her machine… time itself.
If she’s right, then maybe I can bring back hope.

22.08.2034
This may be my last entry.
The time machine is nearly complete. Mother says it can only take me once. Once to the past, once to warn them.
I’ll go back twenty years. I’ll find Goku. I’ll tell him everything. Maybe together they can stop what I couldn’t.
I leave this diary behind for her, in case I never return.
Mother… thank you. Gohan… I’ll carry your will.
		`, modtime: time.Date(2034, 8, 22, 20, 34, 58, 0, time.UTC)},
		"september.txt": {content: `
10.09.2034
The machine stands ready. I tested its power core today. It hummed with energy, unlike anything I’ve felt before.
But the Androids are restless. They must sense something. They destroyed the southern refugee camp. I fought them—
[Gap: two shaky lines, illegible, written mid-battle. The pen cut through the paper.]
—I barely escaped. If they find Capsule Corp, everything ends.

26.09.2034
Tomorrow… tomorrow I leave this ruined world behind. The time machine hums in the corner, ready to tear through years. 2025. I will find Crili there, just like we planned. We’ll attend RomHack together… see what kind of trouble they stir.
But I’m not going there for fun alone. Red Ribbons… their presence lingers like a shadow. I need to spot them. Track them. And Dr. Gelo—he’s the key. He might have left some device at RomHack.
I hope I’m ready. I hope I can… make a difference, even there.

27.09.2034
I needed some place where to store some important information about my time travel... luckily Cyber Saiyan offered to help me with hosting my server under their domain.
What I need from Red Ribbon will be at RomHack 2025... I plugged also that online at redribbon.cybersaiyan.it. I hope some hackers will be able to help me.`,
			modtime: time.Date(2034, 9, 27, 7, 2, 58, 0, time.UTC)},
	},
}

func main() {
	mux := http.NewServeMux()

	// Root index
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		fmt.Fprint(w, indexHTML)
	})

	// Virtual directories (hardcoded). Each dir has a listing and file endpoints.
	mux.HandleFunc("/~bulma/", makeDirHandler("bulma"))
	mux.HandleFunc("/~trunks/", makeDirHandler("trunks"))

	// File endpoints (exact match only). This avoids any path traversal.
	mux.HandleFunc("/~bulma", func(w http.ResponseWriter, r *http.Request) { http.Redirect(w, r, "/~bulma/", http.StatusSeeOther) })
	mux.HandleFunc("/~trunks", func(w http.ResponseWriter, r *http.Request) { http.Redirect(w, r, "/~trunks/", http.StatusSeeOther) })

	addr := ":8000"
	log.Printf("Starting server on %s", addr)
	log.Fatal(http.ListenAndServe(addr, secureHeaders(mux)))
}

// makeDirHandler returns a handler that serves a directory index and files
// from the in-memory site map. It only serves exact file keys under /~name/<file>.
func makeDirHandler(site string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// ensure prefix is exactly /~site/
		prefix := "/~" + site + "/"
		if !strings.HasPrefix(r.URL.Path, prefix) {
			http.NotFound(w, r)
			return
		}

		rel := strings.TrimPrefix(r.URL.Path, prefix) // may be "" or "filename"
		// Normalize and disallow any path tricks
		rel = path.Clean("/" + rel)
		if rel == "/" || rel == "." || rel == "" {
			// Directory listing
			serveListing(w, site, prefix)
			return
		}
		// strip leading '/'
		rel = strings.TrimPrefix(rel, "/")

		files, ok := sites[site]
		if !ok {
			http.NotFound(w, r)
			return
		}
		f, ok := files[rel]
		if !ok {
			http.NotFound(w, r)
			return
		}

		// Set content-type based on extension
		ext := strings.ToLower(path.Ext(rel))
		switch ext {
		case ".html":
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
		case ".svg":
			w.Header().Set("Content-Type", "image/svg+xml; charset=utf-8")
		case ".txt":
			w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		default:
			w.Header().Set("Content-Type", "application/octet-stream")
		}

		// Serve content and a conservative Last-Modified header
		w.Header().Set("Last-Modified", f.modtime.UTC().Format(http.TimeFormat))
		http.ServeContent(w, r, rel, f.modtime, strings.NewReader(f.content))
	}
}

func serveListing(w http.ResponseWriter, site string, prefix string) {
	files, ok := sites[site]
	if !ok {
		http.NotFound(w, nil)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	fmt.Fprintf(w, "<!doctype html><html><head><meta charset=\"utf-8\"><title>%s</title></head><body>", prefix)
	fmt.Fprintf(w, "<h1>Index of %s</h1><hr /><ul>", prefix)
	for name := range files {
		fmt.Fprintf(w, "<li><a href=\"/~%s/%s\">%s</a></li>", site, name, name)
	}
	fmt.Fprint(w, "</ul><hr /><p><i>CapsCorp Content Server - 1.0-beta</i></p></body></html>")
}

// secureHeaders wraps the mux to add a small set of safe headers and to
// disable any unwanted methods.
func secureHeaders(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Deny dangerous methods explicitly
		if r.Method != http.MethodGet && r.Method != http.MethodHead {
			w.Header().Set("Allow", "GET, HEAD")
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("X-Frame-Options", "DENY")
		next.ServeHTTP(w, r)
	})
}
