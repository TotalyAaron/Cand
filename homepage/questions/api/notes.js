import { neon } from "@neondatabase/serverless";
const sql = neon(process.env.DATABASE_URL);

export default async function handler(req, res) {
  try {
    if (req.method === "POST") {
      const { text } = req.body;
      await sql`INSERT INTO notes (text) VALUES (${text})`;
      return res.status(200).json({ message: "Note added" });

    } else if (req.method === "PUT") {
      const { id, text } = req.body;
      await sql`UPDATE notes SET text = ${text} WHERE id = ${id}`;
      return res.status(200).json({ message: "Note updated" });

    } else if (req.method === "DELETE") {
      const { id } = req.body;
      await sql`DELETE FROM notes WHERE id = ${id}`;
      return res.status(200).json({ message: "Note deleted" });

    } else if (req.method === "GET") {
      const { id } = req.query; // use query params, not base/body
      if (id) {
        const row = await sql`SELECT * FROM notes WHERE id = ${id}`;
        return res.status(200).json(row);
      } else {
        const rows = await sql`SELECT * FROM notes ORDER BY id`;
        return res.status(200).json(rows);
      }
    } else {
      return res.status(405).json({ error: "Method not allowed" });
    }
  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: "Internal Server Error" });
  }
}
