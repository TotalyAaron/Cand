import { neon } from "@neondatabase/serverless";
const sql = neon(process.env.DATABASE_URL);
export default async function handler(req, res) {
  if (req.method === "POST") {
    // Insert a new note
    const { text } = req.body;
    await sql`INSERT INTO notes (text) VALUES (${text})`;
    res.status(200).json({ message: "Note added" });

  } else if (req.method === "PUT") {
    // Update an existing note
    const { id, text } = req.body;
    await sql`UPDATE notes SET text = ${text} WHERE id = ${id}`;
    res.status(200).json({ message: "Note updated" });

  } else if (req.method === "DELETE") {
    // Delete a note
    const { id } = req.body;
    await sql`DELETE FROM notes WHERE id = ${id}`;
    res.status(200).json({ message: "Note deleted" });

  } else {
    // Default: read all notes
    const rows = await sql`SELECT * FROM notes ORDER BY id`;
    res.status(200).json(rows);
  }
}
