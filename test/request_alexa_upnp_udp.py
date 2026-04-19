import socket
from urllib.parse import urlparse
import urllib.request
import xml.etree.ElementTree as ET

def discover_upnp_devices(timeout=2):
    ssdp_request = (
        'M-SEARCH * HTTP/1.1\r\n'
        'HOST:239.255.255.250:1900\r\n'
        'MAN:"ssdp:discover"\r\n'
        'MX:1\r\n'
        'ST:ssdp:all\r\n'
        '\r\n'
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.settimeout(timeout)
    sock.sendto(ssdp_request.encode('utf-8'), ('239.255.255.250', 1900))

    locations = []
    try:
        while True:
            data, addr = sock.recvfrom(65507)
            response = data.decode('utf-8', errors='ignore')
            for line in response.split('\r\n'):
                if line.lower().startswith("location:"):
                    location = line.split(":", 1)[1].strip()
                    locations.append(location)
    except socket.timeout:
        pass

    return locations

def fetch_device_description(locations):
    for url in locations:
        parsed = urlparse(url)
        if parsed.port in (80, None):  # só porta 80
            try:
                with urllib.request.urlopen(url, timeout=5) as resp:
                    content = resp.read().decode('utf-8', errors='ignore')
                    # Parse XML para pegar friendlyName
                    try:
                        root = ET.fromstring(content)
                        ns = {'upnp': 'urn:schemas-upnp-org:device-1-0'}
                        fname = root.find('.//upnp:friendlyName', ns)
                        titulo = fname.text if fname is not None else "Sem friendlyName"
                    except Exception:
                        titulo = "XML inválido ou sem friendlyName"

                    print("\n==============================")
                    print(f"### {titulo} ###")
                    print("==============================")
                    print(content[:1000])  # mostra parte do XML
            except Exception as e:
                print(f"Erro ao acessar {url}: {e}")

if __name__ == "__main__":
    locs = discover_upnp_devices()
    if locs:
        fetch_device_description(locs)
    else:
        print("Nenhum dispositivo UPnP encontrado.")
