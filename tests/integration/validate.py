"""Independent ZIP/XML/CLI validation; Python is a test-only dependency.

All generated fixture content is original and dedicated to CC0-1.0.
This script does not import or invoke a ForgeConvert PDF writer.
"""
import pathlib
import subprocess
import sys
import zipfile
import xml.etree.ElementTree as ET


def fixture(content1=b"BT /F1 12 Tf 1 0 0 1 72 720 Tm (Ol\\341 & <Word>) Tj 0 -30 Td [(Texto) -300 (editavel)] TJ ET",
            content2=b"BT /F1 14 Tf 72 720 Td (Segunda pagina) Tj ET", font=b"Helvetica", extra=b"", length_value=None):
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> >>",
        b"<< /Type /Page /Parent 2 0 R /Contents 6 0 R >>",
        b"<< /Type /Page /Parent 2 0 R /Contents 7 0 R >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /" + font + b" /Encoding /WinAnsiEncoding >>",
        b"<< /Length 8 0 R " + extra + b" >>\nstream\n" + content1 + b"\nendstream",
        b"<< /Length " + str(len(content2)).encode() + b" >>\nstream\n" + content2 + b"\nendstream",
        str(len(content1)).encode() if length_value is None else length_value,
    ]
    data = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for index, obj in enumerate(objects, 1):
        offsets.append(len(data))
        data += f"{index} 0 obj\n".encode() + obj + b"\nendobj\n"
    xref = len(data)
    data += b"xref\n0 9\n0000000000 65535 f \n"
    for offset in offsets:
        data += f"{offset:010d} 00000 n \n".encode()
    data += f"trailer\n<< /Size 9 /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode()
    return bytes(data), xref


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()
    work.mkdir(parents=True, exist_ok=True)

    def run(*args, code=0):
        result = subprocess.run([str(executable), *map(str, args)], capture_output=True, text=True)
        assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
        return result

    def write(name, data):
        path = work / name
        path.write_bytes(data)
        return path

    data, prev = fixture()
    input_pdf = write("simple.pdf", data)
    output = work / "simple.docx"
    run("capabilities")
    inspection = run("inspect", input_pdf)
    assert "Olá & <Word>" in inspection.stdout and "Pages processed: 2" in inspection.stdout
    run("convert", input_pdf, "--to", "docx", "--output", output, "--overwrite")
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == {"[Content_Types].xml", "_rels/.rels", "word/document.xml"}
        assert all(info.compress_type == zipfile.ZIP_STORED for info in archive.infolist())
        for part in archive.namelist():
            ET.fromstring(archive.read(part))
        root = ET.fromstring(archive.read("word/document.xml"))
        ns = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
        texts = [item.text for item in root.findall(".//w:t", ns)]
        assert texts == ["Olá & <Word>", "Texto editavel", "Segunda pagina"], texts
        assert len(root.findall(".//w:br[@w:type='page']", ns)) == 1
        relationships = ET.fromstring(archive.read("_rels/.rels"))
        assert relationships[0].attrib["Target"] in archive.namelist()
        assert all(item.attrib.get("{http://www.w3.org/XML/1998/namespace}space") == "preserve" for item in root.findall(".//w:t", ns))

    original = output.read_bytes()
    run("convert", input_pdf, "--to", "docx", "--output", output, code=5)
    assert output.read_bytes() == original
    run("convert", input_pdf, "--to", "txt", "--output", input_pdf, "--overwrite", code=2)
    assert input_pdf.read_bytes() == data
    alias = work / "alias.pdf"
    if alias.exists():
        alias.unlink()
    alias.hardlink_to(input_pdf)
    run("convert", input_pdf, "--to", "docx", "--output", alias, "--overwrite", code=2)
    assert input_pdf.read_bytes() == data
    alias.unlink()
    output_txt = work / "simple.txt"
    run("convert", input_pdf, "--to", "txt", "--output", output_txt, "--overwrite")
    assert output_txt.read_text() == "Olá & <Word>\nTexto editavel\n\f\nSegunda pagina\n"

    # Latest revision replaces an object; older catalog/page entries remain valid.
    new_content = b"BT /F1 12 Tf 72 720 Td (Updated revision) Tj ET"
    offset = len(data)
    revision = data + b"6 0 obj\n<< /Length " + str(len(new_content)).encode() + b" >>\nstream\n" + new_content + b"\nendstream\nendobj\n"
    xref = len(revision)
    revision += f"xref\n6 1\n{offset:010d} 00000 n \ntrailer\n<< /Size 9 /Prev {prev} >>\nstartxref\n{xref}\n%%EOF\n".encode()
    revised = write("revision.pdf", revision)
    assert "Updated revision" in run("inspect", revised).stdout
    # Free entry in newer revision must not revive an older object.
    free_revision = data + f"xref\n6 1\n0000000000 00001 f \ntrailer\n<< /Size 9 /Prev {prev} >>\nstartxref\n{len(data)}\n%%EOF\n".encode()
    run("inspect", write("freed.pdf", free_revision), code=4)

    cases = [("no-text", fixture(b"", b"")[0], 3),
             ("font", fixture(font=b"Courier")[0], 3),
             ("filtered", fixture(extra=b"/Filter /FlateDecode")[0], 3),
             ("truncated", data[:100], 4),
             ("wrong-format", b"hello", 3),
             ("cycle-length", fixture(length_value=b"8 0 R")[0], 4)]
    for name, invalid, code in cases:
        path = write(name + ".pdf", invalid)
        dest = work / (name + ".docx")
        if dest.exists():
            dest.unlink()
        run("convert", path, "--to", "docx", "--output", dest, code=code)
        assert not dest.exists()
    lossy = write("lossy.pdf", fixture(content1=fixture.__defaults__[0] + b" /Image Do")[0])
    run("inspect", lossy, code=3)
    assert "warning:" in run("inspect", lossy, "--allow-lossy").stderr
    run("convert", input_pdf, "--to", "pdf", "--output", output, code=2)
    run("convert", input_pdf, "--to", "docx", "--output", work / "absent" / "x.docx", code=5)
    assert not list(work.glob(".forgeconvert-*")), "Temporary artifacts leaked"
    print("PASS independent ZIP/XML, two-page Unicode conversion, revision precedence, CLI and atomic output checks")


if __name__ == "__main__":
    main()
