{
  description = "Nix flake for NAF";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };
  outputs = { self, nixpkgs }:
    let
      allSystems = [ "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs allSystems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      packages = forAllSystems ({ pkgs }: {
        default = pkgs.callPackage ./default.nix { inherit pkgs; };
      });

      devShells = forAllSystems ({ pkgs }: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.system}.default ];
          # tools for development (modify as needed)
          #          nativeBuildInputs = with pkgs; [ ]; 
        };
      });
    };
}
