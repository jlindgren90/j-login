pkgname=j-login
pkgver=0.1
pkgrel=1
arch=('x86_64')
depends=('gtk2' 'libxss' 'xorg-xrdb' 'xorg-xsetroot')

build() {
    cd ..
    make
}

package() {
    cd ..
    make DESTDIR="${pkgdir}" install
}
