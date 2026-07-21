#include "xsettings.h"

#include <QByteArray>

#include <cstdlib>
#include <cstring>

#include <xcb/xcb.h>

namespace {

quint16 read16(const unsigned char *p, bool isBigEndian) {
    if (isBigEndian) {
        return (quint16(p[0]) << 8 | p[1]);
    } else {
        return (quint16(p[1]) << 8 | p[0]);
    }
}

quint32 read32(const unsigned char *p, bool isBigEndian) {
    if (isBigEndian) {
        return (quint32(p[0]) << 24 | quint32(p[1]) << 16
            | quint32(p[2]) << 8 | p[3]);
    } else {
        return (quint32(p[3]) << 24 | quint32(p[2]) << 16
            | quint32(p[1]) << 8 | p[0]);
    }
}

xcb_atom_t internAtom(xcb_connection_t *conn, const char *name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, qstrlen(name),
        name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie,
        nullptr);

    xcb_atom_t atom = XCB_ATOM_NONE;
    if (reply) {
        atom = reply->atom;
    }

    free(reply);
    return atom;
}

}  // namespace

namespace Wayland {

QString xsettingsString(const QString &key) {
    int screen = 0;
    xcb_connection_t *conn = xcb_connect(nullptr, &screen);
    if (!conn || xcb_connection_has_error(conn)) {
        if (conn) {
            xcb_disconnect(conn);
        }
        return QString();
    }

    QString result;
    xcb_get_property_reply_t *prop = nullptr;

    do {
        const QByteArray sel = "_XSETTINGS_S" + QByteArray::number(screen);
        const xcb_atom_t selAtom = internAtom(conn, sel.constData());
        const xcb_atom_t setAtom = internAtom(conn, "_XSETTINGS_SETTINGS");
        if (selAtom == XCB_ATOM_NONE || setAtom == XCB_ATOM_NONE) {
            break;
        }

        xcb_get_selection_owner_reply_t *own = xcb_get_selection_owner_reply(
            conn, xcb_get_selection_owner(conn, selAtom), nullptr);
        const xcb_window_t owner = own ? own->owner : XCB_WINDOW_NONE;
        free(own);

        if (owner == XCB_WINDOW_NONE) {
            break;
        }

        prop = xcb_get_property_reply(conn, xcb_get_property(conn, 0, owner,
            setAtom, setAtom, 0, 0x4000), nullptr);
        if (!prop) {
            break;
        }

        const int len = xcb_get_property_value_length(prop);
        const unsigned char *d =
            static_cast<const unsigned char *>(xcb_get_property_value(prop));
        if (len < 12) {
            break;
        }

        const bool be = d[0] != 0;
        const quint32 count = read32(d + 8, be);
        const QByteArray want = key.toUtf8();

        int off = 12;
        for (quint32 i = 0; i < count && off + 4 <= len; ++i) {
            const quint8 type = d[off];
            const quint16 nameLen = read16(d + off + 2, be);
            const int nameOff = off + 4;
            if (nameOff + nameLen > len) {
                break;
            }
            const QByteArray name(reinterpret_cast<const char *>(d) + nameOff,
                nameLen);

            int p = (nameOff + nameLen + 3) & ~3;
            p += 4;
            if (p + 4 > len) {
                break;
            }

            if (type == 1) {
                const quint32 vlen = read32(d + p, be);
                const int vOff = p + 4;
                if (vOff + static_cast<int>(vlen) > len) {
                    break;
                }

                if (name == want) {
                    result = QString::fromUtf8(
                        reinterpret_cast<const char *>(d) + vOff, vlen);
                    break;
                }
                p = (vOff + static_cast<int>(vlen) + 3) & ~3;
            } else if (type == 0) {
                p += 4;
            } else if (type == 2) {
                p += 8;
            } else {
                break;
            }
            off = p;
        }
    } while (false);

    free(prop);
    xcb_disconnect(conn);
    return result;
}

}  // namespace Wayland
