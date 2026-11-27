# Maintainer: Veranto
pkgname=tty21
pkgver=0.1.2
pkgrel=1
pkgdesc='Animated terminal blackjack roguelike'
arch=('x86_64' 'aarch64')
url='https://github.com/NotVeranto/tty21'
license=('MIT')
depends=('glibc')
makedepends=('git' 'make' 'gcc')
checkdepends=('python')
source=("git+https://github.com/NotVeranto/tty21.git#tag=v${pkgver}")
sha256sums=('SKIP')

build() {
  cd "${srcdir}/tty21"
  make CC="${CC:-cc}" VERSION="${pkgver}"
}

check() {
  cd "${srcdir}/tty21"
  make test
}

package() {
  cd "${srcdir}/tty21"
  make PREFIX=/usr DESTDIR="${pkgdir}" install
}
