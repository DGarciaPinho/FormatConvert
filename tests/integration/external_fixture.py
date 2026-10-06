"""Optional external fixture/differential test. Never linked to the converter."""
import pathlib
import subprocess
import sys
import zipfile
import xml.etree.ElementTree as ET

exe, gs, pdftotext, source, directory = sys.argv[1:]
work = pathlib.Path(directory)
work.mkdir(parents=True, exist_ok=True)
pdf = work / "independent.pdf"
subprocess.run([gs, "-q", "-dSAFER", "-dBATCH", "-dNOPAUSE", "-sDEVICE=pdfwrite",
                "-dCompatibilityLevel=1.4", "-dCompressPages=false", "-dCompressFonts=false",
                "-dEmbedAllFonts=false", "-dToUnicodeForStdEnc=false",
                "-sOutputFile=" + str(pdf), source], check=True, capture_output=True)
subprocess.run([exe, "convert", str(pdf), "--to", "txt", "--output", str(work / "own.txt"), "--overwrite"], check=True, capture_output=True)
subprocess.run([pdftotext, str(pdf), str(work / "reference.txt")], check=True, capture_output=True)
own = (work / "own.txt").read_text()
reference = (work / "reference.txt").read_text()
assert " ".join(own.split()) == " ".join(reference.split()), (own, reference)
docx = work / "independent.docx"
subprocess.run([exe, "convert", str(pdf), "--to", "docx", "--output", str(docx), "--overwrite"], check=True, capture_output=True)
with zipfile.ZipFile(docx) as archive:
    assert archive.testzip() is None
    xml = ET.fromstring(archive.read("word/document.xml"))
    texts = [node.text for node in xml.findall(".//{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t")]
    assert " ".join(" ".join(texts).split()) == " ".join(reference.split())
print("PASS Ghostscript independent producer, pdftotext differential, DOCX editable text")
