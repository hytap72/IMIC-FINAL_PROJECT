const SNAPSHOT_URL = "https://swimmable-abstract-dreamboat.ngrok-free.dev/snapshot.jpg";

export default async function handler(req, res) {
  try {
    const upstream = await fetch(SNAPSHOT_URL, {
      headers: { "ngrok-skip-browser-warning": "true" },
    });

    if (!upstream.ok) {
      res.status(upstream.status).end();
      return;
    }

    const buf = Buffer.from(await upstream.arrayBuffer());
    res.setHeader("Content-Type", "image/jpeg");
    res.setHeader("Cache-Control", "no-store");
    res.status(200).send(buf);
  } catch (e) {
    res.status(502).json({ error: String(e) });
  }
}
