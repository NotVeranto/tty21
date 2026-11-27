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
source=("git+https://github.com/NotVeranto/tty21.git#commit=5b7dd598c9097049376893c91e1248ea72e06a88")
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
